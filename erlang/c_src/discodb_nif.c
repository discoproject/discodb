#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "erl_nif.h"
#include "discodb.h"
#include "queue.h"

/* Static Erlang Terms */

#define ATOM(Val) (enif_make_atom(env, Val))
#define PAIR(A, B) (enif_make_tuple2(env, A, B))
#define TERM_EQ(lhs, rhs) (enif_compare(lhs, rhs) == 0)
#define ASYNC(R) (PAIR(ATOM_DISCODB, R))

#define ATOM_OK                  ATOM("ok")
#define ATOM_BADARG              ATOM("badarg")
#define ATOM_EALLOC              ATOM("ealloc")
#define ATOM_ECREAT              ATOM("ecreat")
#define ATOM_ERROR               ATOM("error")
#define ATOM_NO                  ATOM("no")
#define ATOM_NULL                ATOM("null")

#define ATOM_DISCODB_CONS        ATOM("discodb_cons")
#define ATOM_NEW                 ATOM("new")
#define ATOM_DDB                 ATOM("ddb")
#define ATOM_ADD                 ATOM("add")
#define ATOM_FINALIZE            ATOM("finalize")

#define ATOM_DISCODB             ATOM("discodb")
#define ATOM_LOAD                ATOM("load")
#define ATOM_LOADS               ATOM("loads")
#define ATOM_DUMP                ATOM("dump")
#define ATOM_DUMPS               ATOM("dumps")
#define ATOM_GET                 ATOM("get")
#define ATOM_ITER                ATOM("iter")
#define ATOM_QUERY               ATOM("query")

#define ATOM_DISABLE_COMPRESSION ATOM("disable_compression")
#define ATOM_KEYS                ATOM("keys")
#define ATOM_VALUES              ATOM("values")
#define ATOM_UNIQUE_VALUES       ATOM("unique_values")
#define ATOM_UNIQUE_ITEMS        ATOM("unique_items")

#define ERROR_BADARG             PAIR(ATOM_ERROR, ATOM_BADARG)
#define ERROR_EALLOC             PAIR(ATOM_ERROR, ATOM_EALLOC)
#define ERROR_ECREAT             PAIR(ATOM_ERROR, ATOM_ECREAT)

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
    return ERROR_ECREAT;
  enif_keep_resource(iter->owner = owner);
  iter->cursor = cursor;
  return make_reference(env, iter);
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

  ERL_NIF_TERM result;
  unsigned char *data = enif_make_new_binary(env, next->length, &result);
  memcpy(data, next->data, next->length);
  return result;
}

static ERL_NIF_TERM
ErlDDBIter_size(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  ErlDDBIter *iter;
  if (!enif_get_resource(env, argv[0], ErlDDBIterType, (void **)&iter))
    return ERROR_BADARG;
  return enif_make_uint64(env, ddb_resultset_size(iter->cursor));
}

static ERL_NIF_TERM
ErlDDBIter_count(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[]) {
  ErlDDBIter *iter;
  if (!enif_get_resource(env, argv[0], ErlDDBIterType, (void **)&iter))
    return ERROR_BADARG;

  int errcode = 0;
  uint64_t n = ddb_cursor_count(iter->cursor, &errcode);
  if (errcode)
    return enif_make_tuple2(env, ATOM_ERROR, enif_make_int(env, errcode));
  return enif_make_uint64(env, n);
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
  ErlNifEnv *env = msg->env;
  if (!(ddb->cons = ddb_cons_new()))
    return ASYNC(ERROR_ECREAT);
  return ASYNC(ATOM_OK);
}

static ERL_NIF_TERM
ErlDiscoDBCons_ddb_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  ErlDDB *odb;
  if (!enif_get_resource(env, msg->term, ErlDDBType, (void **)&odb) || odb->kind != KIND_DB)
    return ASYNC(ERROR_BADARG);
  if (!(ddb->cons = ddb_cons_ddb(odb->db)))
    return ASYNC(make_ddb_error(env, ddb));
  return ASYNC(ATOM_OK);
}

static ERL_NIF_TERM
ErlDiscoDBCons_add_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  int arity;
  const ERL_NIF_TERM *args;
  ErlNifBinary key, val;
  if (!enif_get_tuple(env, msg->term, &arity, &args) || arity != 2)
    return ASYNC(ERROR_BADARG);
  if (!enif_inspect_iolist_as_binary(env, args[0], &key))
    return ASYNC(ERROR_BADARG);
  if (!enif_inspect_iolist_as_binary(env, args[1], &val))
    return ASYNC(ERROR_BADARG);
  if (ddb_cons_add(ddb->cons,
                   &(struct ddb_entry){.data=(char *)key.data, .length=key.size},
                   &(struct ddb_entry){.data=(char *)val.data, .length=val.size}))
    return ASYNC(make_ddb_error(env, ddb));
  return ASYNC(ATOM_OK);
}

static int
ErlDDB_flag(ErlNifEnv *env, const ERL_NIF_TERM atom) {
  if (TERM_EQ(atom, ATOM_DISABLE_COMPRESSION))
    return DDB_OPT_DISABLE_COMPRESSION;
  else if(TERM_EQ(atom, ATOM_UNIQUE_ITEMS))
    return DDB_OPT_UNIQUE_ITEMS;
  return -1;
}

static ERL_NIF_TERM
ErlDiscoDBCons_finalize_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  ErlDDB *new;
  uint64_t n, flags = 0;
  int flag;
  ERL_NIF_TERM head, opts = msg->term;

  if (!enif_is_list(env, opts))
    return ASYNC(ERROR_BADARG);
  while (enif_get_list_cell(env, opts, &head, &opts)) {
    if ((flag = ErlDDB_flag(env, head)) >= 0)
      flags |= flag;
    else
      return ASYNC(ERROR_BADARG);
  }

  if (!(new = ErlDDB_start(env)))
    return ASYNC(ERROR_ECREAT);
  new->kind = KIND_DB;
  if (!(new->buf = ddb_cons_finalize(ddb->cons, &n, flags)))
    return ASYNC(ERROR_EALLOC);
  if (!(new->db = ddb_new()))
    return ASYNC(ERROR_EALLOC);
  if (ddb_loads(new->db, new->buf, n))
    return ASYNC(make_ddb_error(env, new));
  return ASYNC(make_reference(env, new));
}

static ERL_NIF_TERM
ErlDiscoDB_load_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  unsigned size;
  if (!(ddb->db = ddb_new()))
    return ASYNC(ERROR_EALLOC);
  if (enif_get_list_length(env, msg->term, &size)) {
    char name[size + 1];
    if (!enif_get_string(env, msg->term, name, size + 1, ERL_NIF_LATIN1))
      return ASYNC(ERROR_BADARG);
    int fd = open(name, O_RDONLY);
    if (fd < 0)
      return ASYNC(enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, errno_id(errno))));
    if (ddb_load(ddb->db, fd)) { // XXX: offset? always take a tuple, default in erl
      close(fd);
      return ASYNC(make_ddb_error(env, ddb));
    }
    close(fd);
    return ASYNC(ATOM_OK);
  }
  return ASYNC(ERROR_BADARG);
}

static ERL_NIF_TERM
ErlDiscoDB_loads_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  ErlNifBinary bin;
  if (!(ddb->db = ddb_new()))
    return ASYNC(ERROR_EALLOC);
  if (!enif_inspect_iolist_as_binary(env, msg->term, &bin))
    return ASYNC(ERROR_BADARG);
  if (!(ddb->buf = malloc(sizeof(char) * bin.size)))
    return ASYNC(ERROR_EALLOC);
  if (ddb_loads(ddb->db, memcpy(ddb->buf, bin.data, bin.size), bin.size))
    return ASYNC(make_ddb_error(env, ddb));
  return ASYNC(ATOM_OK);
}

static ERL_NIF_TERM
ErlDiscoDB_dump_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  unsigned size;
  if (enif_get_list_length(env, msg->term, &size)) {
    char name[size + 1];
    if (!enif_get_string(env, msg->term, name, size + 1, ERL_NIF_LATIN1))
      return ASYNC(ERROR_BADARG);
    int fd = open(name, O_WRONLY | O_CREAT, 0644);
    if (fd < 0)
      return ASYNC(enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, errno_id(errno))));
    if (ddb_dump(ddb->db, fd)) {
      close(fd);
      return ASYNC(make_ddb_error(env, ddb));
    }
    close(fd);
    return ASYNC(ATOM_OK);
  }
  return ASYNC(ERROR_BADARG);
}

static ERL_NIF_TERM
ErlDiscoDB_dumps_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  uint64_t size;
  char *src;
  if (!(src = ddb_dumps(ddb->db, &size)))
    return ASYNC(make_ddb_error(env, ddb));

  ErlNifBinary bin;
  if (!(enif_alloc_binary(size, &bin))) {
    free(src);
    return ASYNC(ERROR_EALLOC);
  }
  memcpy(bin.data, src, bin.size);
  free(src);
  return ASYNC(enif_make_binary(env, &bin));
}

static ERL_NIF_TERM
ErlDiscoDB_get_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  ErlNifBinary key;
  if (!enif_inspect_iolist_as_binary(env, msg->term, &key))
    return ASYNC(ERROR_BADARG);

  struct ddb_cursor *cursor = NULL;
  if (!(cursor = ddb_getitem(ddb->db, &(struct ddb_entry) {.data=(char *)key.data, .length=key.size})))
    return ASYNC(make_ddb_error(env, ddb));
  return ASYNC(ErlDDBIter_new(env, ddb, cursor));
}

static ERL_NIF_TERM
ErlDiscoDB_iter_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  struct ddb_cursor *cursor = NULL;
  if (TERM_EQ(msg->term, ATOM_KEYS))
    cursor = ddb_keys(ddb->db);
  else if (TERM_EQ(msg->term, ATOM_VALUES))
    cursor = ddb_values(ddb->db);
  else if (TERM_EQ(msg->term, ATOM_UNIQUE_VALUES))
    cursor = ddb_unique_values(ddb->db);
  else
    return ASYNC(ERROR_BADARG);

  if (cursor == NULL)
    return ASYNC(make_ddb_error(env, ddb));
  return ASYNC(ErlDDBIter_new(env, ddb, cursor));
}

static void
free_ddb_query_clauses(struct ddb_query_clause *clauses, uint32_t num_clauses) {
  int i;
  if (clauses) {
    for (i = 0; i < num_clauses; i++)
      if (clauses[i].terms)
        free(clauses[i].terms);
    free(clauses);
  }
}

static ERL_NIF_TERM
ErlDiscoDB_query_async(ErlDDB *ddb, Message *msg) {
  ErlNifEnv *env = msg->env;
  ErlNifBinary literal;
  ERL_NIF_TERM clause, term, clauses = msg->term;
  unsigned nclauses = 0, i, j;
  struct ddb_query_clause *ddb_clauses = NULL;
  struct ddb_cursor *cursor = NULL;

  if (!enif_get_list_length(env, clauses, &nclauses))
    goto badarg;

  if (!(ddb_clauses = calloc(nclauses, sizeof(struct ddb_query_clause))))
    goto ecreat;
  for (i = 0; enif_get_list_cell(env, clauses, &clause, &clauses); i++) {
    if (!enif_get_list_length(env, clause, &ddb_clauses[i].num_terms))
      goto badarg;

    if (!(ddb_clauses[i].terms = calloc(ddb_clauses[i].num_terms, sizeof(struct ddb_query_term))))
      goto ecreat;
    for (j = 0; enif_get_list_cell(env, clause, &term, &clause); j++) {
      int arity;
      const ERL_NIF_TERM *pair;
      if (enif_get_tuple(env, term, &arity, &pair)) {
        if (!(arity == 2 && TERM_EQ(pair[0], ATOM_NO)))
          goto badarg;
        ddb_clauses[i].terms[j].nnot = 1;
        term = pair[1];
      }
      if (!enif_inspect_iolist_as_binary(env, term, &literal))
        goto badarg;
      ddb_clauses[i].terms[j].key.data = (char *) literal.data;
      ddb_clauses[i].terms[j].key.length = literal.size;
    }
  }

  if (!(cursor = ddb_query(ddb->db, ddb_clauses, nclauses)))
    goto equery;
  free_ddb_query_clauses(ddb_clauses, nclauses);
  return ASYNC(ErlDDBIter_new(env, ddb, cursor));
 badarg:
  free_ddb_query_clauses(ddb_clauses, nclauses);
  return ASYNC(ERROR_BADARG);
 ecreat:
  free_ddb_query_clauses(ddb_clauses, nclauses);
  return ASYNC(ERROR_ECREAT);
 equery:
  free_ddb_query_clauses(ddb_clauses, nclauses);
  return ASYNC(make_ddb_error(env, ddb));
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
    else if (TERM_EQ(argv[1], ATOM_DDB))
      queue_push(ddb->msgs, Message_new(env, &ErlDiscoDBCons_ddb_async, argv[2]));
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
    {"count", 1, ErlDDBIter_count},
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
  return 0;
}

ERL_NIF_INIT(discodb_nif, nif_funcs, &on_load, NULL, NULL, NULL);
