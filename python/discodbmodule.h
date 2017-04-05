#include "discodb.h"

typedef struct {
    PyObject_VAR_HEAD
    PyObject   *obuffer;
    char       *cbuffer;
    struct ddb *discodb;
} DiscoDB;

typedef struct {
    PyObject_HEAD
    PyTypeObject    *ddb_type;
    struct ddb_cons *ddb_cons;
} DiscoDBConstructor;

typedef struct {
    PyObject_HEAD
    DiscoDB           *owner;
    struct ddb_cursor *cursor;
} DiscoDBIter;

typedef struct {
    PyObject_HEAD
    struct ddb_view *view;
} DiscoDBView;

/* General Object Protocol */

static PyObject * DiscoDB_new     (PyTypeObject *, PyObject *, PyObject *);
static void       DiscoDB_dealloc (DiscoDB *);

/* Mapping Formal / Informal Protocol */

static int        DiscoDB_contains     (DiscoDB *,      PyObject *);
static PyObject * DiscoDB_getitem      (DiscoDB *,      PyObject *);
static PyObject * DiscoDB_keys         (DiscoDB *);
static PyObject * DiscoDB_values       (DiscoDB *);
static PyObject * DiscoDB_unique_values(DiscoDB *);
static PyObject * DiscoDB_query        (DiscoDB *, PyObject *, PyObject *);

/* Serialization / Deserialization Informal Protocol */

static PyObject * DiscoDB_dumps   (DiscoDB *);
static PyObject * DiscoDB_dump    (DiscoDB *, PyObject *);
static PyObject * DiscoDB_loads   (PyTypeObject *, PyObject *);
static PyObject * DiscoDB_load    (PyTypeObject *, PyObject *);

/* DiscoDB Constructor Type */

static PyTypeObject DiscoDBConstructorType;

static PyObject * DiscoDBConstructor_new     (PyTypeObject *,       PyObject *, PyObject *);
static void       DiscoDBConstructor_dealloc (DiscoDBConstructor *);
static PyObject * DiscoDBConstructor_add     (DiscoDBConstructor *, PyObject *);
static PyObject * DiscoDBConstructor_merge   (DiscoDBConstructor *, PyObject *);
static PyObject * DiscoDBConstructor_merge_with_explicit_value   (DiscoDBConstructor *, PyObject *);
static PyObject * DiscoDBConstructor_finalize(DiscoDBConstructor *, PyObject *, PyObject *);

/* DiscoDB Iterator Types */

static PyTypeObject DiscoDBIterType;

static PyObject * DiscoDBIter_new      (PyTypeObject *, DiscoDB *, struct ddb_cursor *);
static void       DiscoDBIter_dealloc  (DiscoDBIter *);
static PyObject * DiscoDBIter_count    (DiscoDBIter *);
static PyObject * DiscoDBIter_size     (DiscoDBIter *);
static PyObject * DiscoDBIter_iternext (DiscoDBIter *);

/* DiscoDB View Types */

static PyTypeObject DiscoDBViewType;

static PyObject * DiscoDBView_new     (PyTypeObject *, PyObject *, PyObject *);
static void       DiscoDBView_dealloc (DiscoDBView *);
static Py_ssize_t DiscoDBView_len     (DiscoDBView *);

/* ddb helpers */

static struct ddb              *ddb_alloc               (void);
static struct ddb_cons         *ddb_cons_alloc          (void);
static struct ddb_query_clause *ddb_query_clause_alloc  (size_t);
static struct ddb_query_term   *ddb_query_term_alloc    (size_t);
static        void              ddb_cons_dealloc        (struct ddb_cons *);
static        void              ddb_cursor_dealloc      (struct ddb_cursor *);
static        void              ddb_query_clause_dealloc(struct ddb_query_clause *, uint32_t);
static        int               ddb_has_error           (struct ddb *);
static        int               ddb_string_to_entry     (PyObject *, struct ddb_entry *);

#define DiscoDB_CLEAR(op) do { free(op); op = NULL; } while(0)
