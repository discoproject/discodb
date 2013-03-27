
#include <string.h>
#include <stdint.h>

#include <cmph.h>

#include <ddb_map.h>
#include <ddb_cmph.h>

struct ddb_cmph_data {
  const struct ddb_map *map;
  struct ddb_map_cursor *cursor;
  struct ddb_entry *key;
  char *buf;
  uint32_t *hash_failed;
};

void xdispose(void *data, char *key, cmph_uint32 l) {}
void xrewind(void *data) {
  struct ddb_cmph_data *d = (struct ddb_cmph_data *) data;
  ddb_map_cursor_free(d->cursor);
  d->cursor = ddb_map_cursor_new(d->map);
}
int xread(void *data, char **p, cmph_uint32 *len) {
  struct ddb_cmph_data *d = (struct ddb_cmph_data *) data;
  struct ddb_map_cursor *c = d->cursor;
  struct ddb_entry key = *d->key;
  uint32_t hash_failed = *d->hash_failed, buf_len = 0;

  if (c) {
    ddb_map_next_str(c, &key);
    if (key.length > buf_len){
      buf_len = key.length;
      if (!(d->buf = realloc(d->buf, buf_len)))
        hash_failed = 1;
    }
  } else {
    hash_failed = 1;
  }

  if (hash_failed){
    *len = 0;
    *p = NULL;
  } else {
    memcpy(d->buf, key.data, key.length);
    *len = key.length;
    *p = d->buf;
  }
  return *len;
}

char *ddb_build_cmph(const struct ddb_map *keys_map, uint32_t *size)
{
    char *buf = NULL;
    uint32_t hash_failed = 0;
    struct ddb_entry key;
    struct ddb_cmph_data data = {.map=keys_map,
                                 .cursor=ddb_map_cursor_new(keys_map),
                                 .key=&key,
                                 .buf=buf,
                                 .hash_failed=&hash_failed};

    cmph_io_adapter_t r;
    r.data = &data;
    r.nkeys = ddb_map_num_items(keys_map);
    r.read = xread;
    r.dispose = xdispose;
    r.rewind = xrewind;

    cmph_config_t *cmph = cmph_config_new(&r);
    cmph_config_set_algo(cmph, CMPH_CHD);

    if (getenv("DDB_DEBUG_CMPH"))
        cmph_config_set_verbosity(cmph, 5);

    char *hash = NULL;
    cmph_t *g = cmph_new(cmph);
    *size = 0;
    if (g && !hash_failed){
        *size = cmph_packed_size(g);
        if ((hash = malloc(*size)))
            cmph_pack(g, hash);
    }
    if (g)
        cmph_destroy(g);
    ddb_map_cursor_free(data.cursor);
    cmph_config_destroy(cmph);
    free(buf);
    return hash;
}
