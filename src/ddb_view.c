
#include <stdlib.h>

#include <ddb_internal.h>
#include <ddb_map.h>

struct ddb_view *ddb_create_view(struct ddb *db,
                                 const struct ddb_entry *view_values,
                                 uint32_t num_view_values)
{
    struct ddb_map *map = ddb_map_new(num_view_values);
    struct ddb_cursor *cursor = ddb_unique_values(db);
    struct ddb_view *view = malloc(4 + num_view_values * sizeof(valueid_t));
    const struct ddb_entry *value;
    valueid_t id = 0;
    uint32_t i;
    int err;

    if (!(map && cursor && view))
        goto err;

    for (i = 0; i < num_view_values; i++){
        if (!ddb_map_insert_str(map, &view_values[i]))
            goto err;
    }

    view->num_values = 0;
    while (num_view_values && (value = ddb_next(cursor, &err))){
        ++id;
        if (ddb_map_lookup_str(map, value)){
            view->values[view->num_values++] = id;
            --num_view_values;
        }
    }

    ddb_free_cursor(cursor);
    ddb_map_free(map);
    return view;
err:
    ddb_free_view(view);
    ddb_free_cursor(cursor);
    ddb_map_free(map);
    return NULL;
}

int ddb_free_view(struct ddb_view *view)
{
    free(view);
    return 0;
}

uint32_t ddb_view_size(const struct ddb_view *view)
{
    return view->num_values;
}
