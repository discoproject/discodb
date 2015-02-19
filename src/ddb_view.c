
#include <stdlib.h>

#include <ddb_internal.h>
#include <ddb_map.h>

struct ddb_view_cons *ddb_view_cons_new()
{
    struct ddb_view_cons *cons;
    if (!(cons = malloc(sizeof(struct ddb_view_cons))))
        return NULL;
    if (!(cons->map = ddb_map_new(DDB_MAX_NUM_VALUES))){
        free(cons);
        return NULL;
    }
    return cons;
}

int ddb_view_cons_add(const struct ddb_view_cons *cons,
                      const struct ddb_entry *value)
{
    return ddb_map_insert_str(cons->map, value) == NULL;
}

struct ddb_view *ddb_view_cons_finalize(const struct ddb_view_cons *cons,
                                        struct ddb *db)
{
    uint32_t n = ddb_map_num_items(cons->map);
    struct ddb_cursor *cursor = ddb_unique_values(db);
    struct ddb_view *view = malloc(4 + n * sizeof(valueid_t));
    const struct ddb_entry *value;
    valueid_t id = 0;
    int err = 0;

    if (!(cursor && view))
        goto err;

    view->num_values = 0;
    while (n && (value = ddb_next(cursor, &err))){
        ++id;
        if (ddb_map_lookup_str(cons->map, value)){
            view->values[view->num_values++] = id;
            --n;
        }
    }
    if (!err){
        ddb_free_cursor(cursor);
        return view;
    }
err:
    ddb_view_free(view);
    ddb_free_cursor(cursor);
    return NULL;
}

void ddb_view_cons_free(struct ddb_view_cons *cons)
{
    if (cons){
        ddb_map_free(cons->map);
        free(cons);
    }
}

void ddb_view_free(struct ddb_view *view)
{
    free(view);
}

uint32_t ddb_view_size(const struct ddb_view *view)
{
    return view->num_values;
}
