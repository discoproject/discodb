#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "erl_nif.h"
#include "discodb.h"
#include "queue.h"

/* Static Erlang Terms */

#define ATOM(Id, Value) { Id = enif_make_atom(env, Value); }
#define TERM_EQ(lhs, rhs) (enif_compare(lhs, rhs) == 0)

static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_BADARG;
static ERL_NIF_TERM ATOM_EALLOC;
static ERL_NIF_TERM ATOM_ECREAT;
static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_NULL;

static ERL_NIF_TERM ATOM_DISCODB_CONS;
static ERL_NIF_TERM ATOM_NEW;
static ERL_NIF_TERM ATOM_ADD;
static ERL_NIF_TERM ATOM_FINALIZE;

static ERL_NIF_TERM ATOM_DISCODB;
static ERL_NIF_TERM ATOM_LOAD;
static ERL_NIF_TERM ATOM_LOADS;
static ERL_NIF_TERM ATOM_DUMP;
static ERL_NIF_TERM ATOM_DUMPS;
static ERL_NIF_TERM ATOM_GET;
static ERL_NIF_TERM ATOM_ITER;
static ERL_NIF_TERM ATOM_QUERY;

static ERL_NIF_TERM ATOM_DISABLE_COMPRESSION;
static ERL_NIF_TERM ATOM_KEYS;
static ERL_NIF_TERM ATOM_VALUES;
static ERL_NIF_TERM ATOM_UNIQUE_VALUES;
static ERL_NIF_TERM ATOM_UNIQUE_ITEMS;

static ERL_NIF_TERM ERROR_BADARG;
static ERL_NIF_TERM ERROR_EALLOC;

/* DiscoDB Types */

typedef enum { KIND_CONS, KIND_DB } ErlDDBKind;
typedef struct {
  ErlNifTid tid;
  ErlNifThreadOpts *opts;
  queue *msgs;
  char *buf;
  ErlDDBKind kind;
  union {
    struct ddb_cons *cons;
    struct ddb *db;
  };
} ErlDDB;

typedef struct {
  ErlDDB *owner;
  struct ddb_cursor *cursor;
} ErlDDBIter;

static ErlNifResourceType *ErlDDBType;
static ErlNifResourceType *ErlDDBIterType;

typedef struct Message Message;
typedef ERL_NIF_TERM (*ErlDDBFn)(ErlDDB *, Message *);

struct Message {
  ErlNifEnv *env;
  ErlNifPid from;
  ErlDDBFn func;
  ERL_NIF_TERM term;
};

/* Support Functions */

static char *
errno_id(int error) {
  switch (error) {
    case EACCES: return "eacces";
    case EAGAIN: return "eagain";
    case EEXIST: return "eexist";
    case EINVAL: return "einval";
    case EISDIR: return "eisdir";
    case EMFILE: return "emfile";
    case ENFILE: return "enfile";
    case ENOENT: return "enoent";
    case ENOMEM: return "enomem";
    case ENOSPC: return "enospc";
  }
  return "unknown";
}

static ERL_NIF_TERM
make_reference(ErlNifEnv *env, void *res) {
  ERL_NIF_TERM ref = enif_make_resource(env, res);
  enif_release_resource(res);
  return ref;
}

static ERL_NIF_TERM
make_ddb_error(ErlNifEnv *env, ErlDDB *ddb) {
  const char *errstr;
  if (ddb_error(ddb->db, &errstr))
    return enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, errstr));
  return enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, "unknown"));
}

/* DiscoDB Implementation */

static void
Message_free(Message *msg) {
  if (msg->env)
    enif_free_env(msg->env);
  enif_free(msg);
}

static Message *
Message_new(ErlNifEnv *env, ErlDDBFn func, ERL_NIF_TERM term) {
  Message *msg;
  if (!(msg = (Message *)enif_alloc(sizeof(Message))))
    return NULL;

  if (!(msg->env = enif_alloc_env())) {
    Message_free(msg);
    return NULL;
  }

  if (env)
    enif_self(env, &msg->from);

  msg->func = func;
  msg->term = term ? enif_make_copy(msg->env, term) : 0;
  return msg;
}

static void
ErlDDBIter_free(ErlNifEnv *env, void *res) {
  ErlDDBIter *iter = (ErlDDBIter *)res;
  enif_release_resource(iter->owner);
  ddb_free_cursor(iter->cursor);
}

static ERL_NIF_TERM
ErlDDBIter_new(ErlNifEnv *env, ErlDDB *owner, struct ddb_cursor *cursor) {
  ErlDDBIter *iter;
  if (!(iter = enif_alloc_resource(ErlDDBIterType, sizeof(ErlDDBIter))))
    goto error;
  enif_keep_resource(iter->owner = owner);
  iter->cursor = cursor;
  return make_reference(env, iter);

 error:
  if (iter)
    enif_release_resource(iter);
  return enif_make_tuple2(env, ATOM_ERROR, ATOM_ECREAT);
}

static ERL_NIF_TERM
ErlDDBIter_next(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  ErlDDBIter *iter;
  if (!enif_get_resource(env, argv[0], ErlDDBIterType, (void **)&iter))
    return ERROR_BADARG;

  int errcode;
  const struct ddb_entry *next = ddb_next(iter->cursor, &errcode);
  if (errcode)
    return enif_make_tuple2(env, ATOM_ERROR, enif_make_int(env, errcode));
  if (next == NULL)
    return ATOM_NULL;

  return enif_make_resource_binary(env, iter->owner, next->data, next->length);
}

static ERL_NIF_TERM
ErlDDBIter_size(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  ErlDDBIter *iter;
  if (!enif_get_resource(env, argv[0], ErlDDBIterType, (void **)&iter))
    return ERROR_BADARG;
  return enif_make_uint64(env, ddb_resultset_size(iter->cursor));
}

static void
ErlDDB_free(ErlNifEnv *env, void *res) {
  ErlDDB *ddb = (ErlDDB *)res;
  Message *stop = Message_new(NULL, NULL, 0);

  queue_push(ddb->msgs, stop);

  enif_thread_join(ddb->tid, NULL);
  enif_thread_opts_destroy(ddb->opts);

  queue_free(ddb->msgs);

  if (ddb->kind == KIND_CONS && ddb->cons)
    ddb_cons_free(ddb->cons);
  else if (ddb->kind == KIND_DB && ddb->db)
    ddb_free(ddb->db);
  if (ddb->buf)
    free(ddb->buf);
}

static void *
ErlDDB_run(void *arg) {
  ErlDDB *ddb = (ErlDDB *)arg;
  int done = 0;

  while (!done) {
    Message *msg = queue_pop(ddb->msgs);
    if (msg->func)
      enif_send(NULL, &msg->from, msg->env, msg->func(ddb, msg));
    else
      done = 1;
    Message_free(msg);
  }

  return NULL;
}

static ErlDDB *
ErlDDB_start(ErlNifEnv *env) {
  ErlDDB *ddb;
  if (!(ddb = enif_alloc_resource(ErlDDBType, sizeof(ErlDDB))))
    goto error;
  if (!(ddb = memset(ddb, 0, sizeof(ErlDDB))))
    goto error;
  if (!(ddb->msgs = queue_new()))
    goto error;
  if (!(ddb->opts = enif_thread_opts_create("discodb_opts")))
    goto error;
  if (enif_thread_create("discodb", &ddb->tid, &ErlDDB_run, ddb, ddb->opts))
    goto error;
  return ddb;

 error:
  if (ddb)
    enif_release_resource(ddb);
  return NULL;
}

static ERL_NIF_TERM
ErlDiscoDBCons_new_async(ErlDDB *ddb, Message *msg) {
  if (!(ddb->cons = ddb_cons_new()))
    return enif_make_tuple2(msg->env, ATOM_ERROR, ATOM_ECREAT);
  return ATOM_OK;
}

static ERL_NIF_TERM
ErlDiscoDBCons_add_async(ErlDDB *ddb, Message *msg) {
  int arity;
  const ERL_NIF_TERM *args;
  ErlNifBinary key, val;
  if (!enif_get_tuple(msg->env, msg->term, &arity, &args) || arity != 2)
    return ERROR_BADARG;
  if (!enif_inspect_iolist_as_binary(msg->env, args[0], &key))
    return ERROR_BADARG;
  if (!enif_inspect_iolist_as_binary(msg->env, args[1], &val))
    return ERROR_BADARG;
  if (ddb_cons_add(ddb->cons,
                   &(struct ddb_entry){.data=(char *)key.data, .length=key.size},
                   &(struct ddb_entry){.data=(char *)val.data, .length=val.size}))
    return make_ddb_error(msg->env, ddb);
  return ATOM_OK;
}

static ERL_NIF_TERM
ErlDiscoDBCons_finalize_async(ErlDDB *ddb, Message *msg) {
  ErlDDB *new;
  uint64_t n, flags = 0; // XXX: implement flags
  if (!(new = ErlDDB_start(msg->env)))
    return enif_make_tuple2(msg->env, ATOM_ERROR, ATOM_ECREAT);
  new->kind = KIND_DB;
  if (!(new->buf = ddb_cons_finalize(ddb->cons, &n, flags)))
    return ERROR_EALLOC;
  if (!(new->db = ddb_new()))
    return ERROR_EALLOC;
  if (ddb_loads(new->db, new->buf, n))
    return make_ddb_error(msg->env, new);
  return make_reference(msg->env, new);
}

static ERL_NIF_TERM
ErlDiscoDB_load_async(ErlDDB *ddb, Message *msg) {
  unsigned size;
  if (!(ddb->db = ddb_new()))
    return ERROR_EALLOC;
  if (enif_get_list_length(msg->env, msg->term, &size)) {
    char name[size + 1];
    if (!enif_get_string(msg->env, msg->term, name, size + 1, ERL_NIF_LATIN1))
      return ERROR_BADARG;
    int fd = open(name, O_RDONLY);
    if (fd < 0)
      return enif_make_tuple2(msg->env, ATOM_ERROR, enif_make_atom(msg->env, errno_id(errno)));
    if (ddb_load(ddb->db, fd)) { // XXX: offset? always take a tuple, default in erl
      close(fd);
      return make_ddb_error(msg->env, ddb);
    }
    close(fd);
    return ATOM_OK;
  }
  return ERROR_BADARG;
}

static ERL_NIF_TERM
ErlDiscoDB_loads_async(ErlDDB *ddb, Message *msg) {
  ErlNifBinary bin;
  if (!(ddb->db = ddb_new()))
    return ERROR_EALLOC;
  if (!enif_inspect_iolist_as_binary(msg->env, msg->term, &bin))
    return ERROR_BADARG;
  if (!(ddb->buf = malloc(sizeof(char) * bin.size)))
    return ERROR_EALLOC;
  if (ddb_loads(ddb->db, memcpy(ddb->buf, bin.data, bin.size), bin.size))
    return make_ddb_error(msg->env, ddb);
  return ATOM_OK;
}

static ERL_NIF_TERM
ErlDiscoDB_dump_async(ErlDDB *ddb, Message *msg) {
  unsigned size;
  if (enif_get_list_length(msg->env, msg->term, &size)) {
    char name[size + 1];
    if (!enif_get_string(msg->env, msg->term, name, size + 1, ERL_NIF_LATIN1))
      return ERROR_BADARG;
    int fd = open(name, O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
      return enif_make_tuple2(msg->env, ATOM_ERROR, enif_make_atom(msg->env, errno_id(errno)));
    if (ddb_dump(ddb->db, fd)) {
      close(fd);
      return make_ddb_error(msg->env, ddb);
    }
    close(fd);
    return ATOM_OK;
  }
  return ERROR_BADARG;
}

static ERL_NIF_TERM
ErlDiscoDB_dumps_async(ErlDDB *ddb, Message *msg) {
  uint64_t size;
  char *src;
  if (!(src = ddb_dumps(ddb->db, &size)))
    return make_ddb_error(msg->env, ddb);

  ErlNifBinary bin;
  if (!(enif_alloc_binary(size, &bin))) {
    free(src);
    return ERROR_EALLOC;
  }
  memcpy(bin.data, src, bin.size);
  free(src);
  return enif_make_binary(msg->env, &bin);
}

static ERL_NIF_TERM
ErlDiscoDB_get_async(ErlDDB *ddb, Message *msg) {
  ErlNifBinary key;
  if (!enif_inspect_iolist_as_binary(msg->env, msg->term, &key))
    return ERROR_BADARG;

  struct ddb_cursor *cursor = NULL;
  if (!(cursor = ddb_getitem(ddb->db, &(struct ddb_entry) {.data=(char *)key.data, .length=key.size})))
    return make_ddb_error(msg->env, ddb);

  if (ddb_notfound(cursor)) {
    ddb_free_cursor(cursor);
    return ATOM_ERROR;
  }

  return ErlDDBIter_new(msg->env, ddb, cursor);
}

static ERL_NIF_TERM
ErlDiscoDB_iter_async(ErlDDB *ddb, Message *msg) {
  struct ddb_cursor *cursor = NULL;
  if (TERM_EQ(msg->term, ATOM_KEYS))
    cursor = ddb_keys(ddb->db);
  else if (TERM_EQ(msg->term, ATOM_VALUES))
    cursor = ddb_values(ddb->db);
  else if (TERM_EQ(msg->term, ATOM_UNIQUE_VALUES))
    cursor = ddb_unique_values(ddb->db);
  else
    return ERROR_BADARG;

  if (cursor == NULL)
    return make_ddb_error(msg->env, ddb);
  return ErlDDBIter_new(msg->env, ddb, cursor);
}

static ERL_NIF_TERM
ErlDiscoDB_query_async(ErlDDB *ddb, Message *msg) {
  return ATOM_OK;
}

static ERL_NIF_TERM
ErlDDB_init(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  ErlDDB *ddb;
  if (!(ddb = ErlDDB_start(env)))
    return ERROR_EALLOC;

  if (TERM_EQ(argv[0], ATOM_DISCODB_CONS))
    ddb->kind = KIND_CONS;
  else if (TERM_EQ(argv[0], ATOM_DISCODB))
    ddb->kind = KIND_DB;
  else
    goto badarg;

  if (ddb->kind == KIND_CONS) {
    if (TERM_EQ(argv[1], ATOM_NEW))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDBCons_new_async, argv[2]));
    else
      goto badarg;
  } else if (ddb->kind == KIND_DB) {
    if (TERM_EQ(argv[1], ATOM_LOAD))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_load_async, argv[2]));
    else if (TERM_EQ(argv[1], ATOM_LOADS))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_loads_async, argv[2]));
    else
      goto badarg;
  }
  return make_reference(env, ddb);

 badarg:
  if (ddb)
    enif_release_resource(ddb);
  return ERROR_BADARG;
}

static ERL_NIF_TERM
ErlDDB_call(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  ErlDDB *ddb;
  if (!enif_get_resource(env, argv[0], ErlDDBType, (void **)&ddb))
    return ERROR_BADARG;

  if (ddb->kind == KIND_CONS) {
    if (TERM_EQ(argv[1], ATOM_ADD))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDBCons_add_async, argv[2]));
    else if (TERM_EQ(argv[1], ATOM_FINALIZE))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDBCons_finalize_async, argv[2]));
    else
      return ERROR_BADARG;
  } else if (ddb->kind == KIND_DB) {
    if (TERM_EQ(argv[1], ATOM_DUMP))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_dump_async, argv[2]));
    else if (TERM_EQ(argv[1], ATOM_DUMPS))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_dumps_async, argv[2]));
    else if (TERM_EQ(argv[1], ATOM_GET))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_get_async, argv[2]));
    else if (TERM_EQ(argv[1], ATOM_ITER))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_iter_async, argv[2]));
    else if (TERM_EQ(argv[1], ATOM_QUERY))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDB_query_async, argv[2]));
    else
      return ERROR_BADARG;
  }
  return argv[0];
}

/* NIF Initialization */

static ErlNifFunc nif_funcs[] =
  {
    {"init", 3, ErlDDB_init},
    {"call", 3, ErlDDB_call},
    {"next", 1, ErlDDBIter_next},
    {"size", 1, ErlDDBIter_size},
  };

static int
on_load(ErlNifEnv *env, void **priv_data, ERL_NIF_TERM load_info) {
  ErlNifResourceFlags flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  ErlDDBType = enif_open_resource_type(env, NULL, "discodb", &ErlDDB_free, flags, NULL);
  if (ErlDDBType == NULL)
    return -1;
  ErlDDBIterType = enif_open_resource_type(env, NULL, "discodb_iter", &ErlDDBIter_free, flags, NULL);
  if (ErlDDBIterType == NULL)
    return -1;

  ATOM(ATOM_OK, "ok");
  ATOM(ATOM_BADARG, "badarg");
  ATOM(ATOM_EALLOC, "ealloc");
  ATOM(ATOM_ECREAT, "ecreat");
  ATOM(ATOM_ERROR, "error");
  ATOM(ATOM_NULL, "null");

  ATOM(ATOM_DISCODB_CONS, "discodb_cons");
  ATOM(ATOM_NEW, "new");
  ATOM(ATOM_ADD, "add");
  ATOM(ATOM_FINALIZE, "finalize");

  ATOM(ATOM_DISCODB, "discodb");
  ATOM(ATOM_LOAD, "load");
  ATOM(ATOM_LOADS, "loads");
  ATOM(ATOM_DUMP, "dump");
  ATOM(ATOM_DUMPS, "dumps");
  ATOM(ATOM_GET, "get");
  ATOM(ATOM_ITER, "iter");
  ATOM(ATOM_QUERY, "query");

  ATOM(ATOM_DISABLE_COMPRESSION, "disable_compression");
  ATOM(ATOM_KEYS, "keys");
  ATOM(ATOM_VALUES, "values");
  ATOM(ATOM_UNIQUE_VALUES, "unique_values");
  ATOM(ATOM_UNIQUE_ITEMS, "unique_items");

  ERROR_BADARG = enif_make_tuple2(env, ATOM_ERROR, ATOM_BADARG);
  ERROR_EALLOC = enif_make_tuple2(env, ATOM_ERROR, ATOM_EALLOC);

  return 0;
}

ERL_NIF_INIT(discodb_nif, nif_funcs, &on_load, NULL, NULL, NULL);
