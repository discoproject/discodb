--[[
   Basic discodb wrapper for Lua.
   Assuming you have discodb library loaded / linked:

     local discodb = require("discodb")
     local db = discodb.open("/path/to/ddb")
     local iter = db:keys()
     for i = 1,#iter do
        print(i, iter:next())
     end

   etc.

   TODO:
    - wrap CNF queries
    - discodb construction (currently read-only)
    - report actual errors coming from discodb
]]

local ffi = require("ffi")

ffi.cdef[[
int fileno(void *FILE);

struct ddb;
struct ddb_cursor;
struct ddb_entry {
    const char *data;
    uint32_t length;
};

struct ddb *ddb_new();
int ddb_load(struct ddb *db, int fd);

struct ddb_cursor *ddb_keys(struct ddb *db);
struct ddb_cursor *ddb_values(struct ddb *db);
struct ddb_cursor *ddb_unique_values(struct ddb *db);
struct ddb_cursor *ddb_getitem(struct ddb *db, const struct ddb_entry *key);

void ddb_free(struct ddb *db);
int ddb_error(const struct ddb *db, const char **errstr);
int ddb_free_cursor(struct ddb_cursor *cur);
int ddb_notfound(const struct ddb_cursor *c);

const struct ddb_entry *ddb_next(struct ddb_cursor *cur, int *errcode);
uint64_t ddb_resultset_size(const struct ddb_cursor *cur);
uint64_t ddb_cursor_count(struct ddb_cursor *c, int *err);
]]

local Entry = {
   new = function (str)
      local entry = ffi.new('struct ddb_entry')
      if str then
         entry:store(str)
      end
      return entry
   end,

   __index = {
      store = function (entry, str)
         entry.data = str
         entry.length = #str
         return entry
      end
   },

   __tostring = function (entry)
      return ffi.string(entry.data, entry.length)
   end
}

local Cursor = {
   __index = {
      next = function (cursor)
         local err = ffi.new('int[1]')
         local entry = ffi.C.ddb_next(cursor, err)
         if err[0] > 0 then
            error("ddb_next")
         end
         return entry
      end,

      size = function (cursor)
         return ffi.C.ddb_resultset_size(cursor)
      end,

      count = function (cursor)
         local err = ffi.new('int[1]')
         local count = ffi.C.ddb_cursor_count(cursor)
         if err[0] > 0 then
            error("ddb_count")
         end
         return count
      end,

      notfound = function (cursor)
         return ffi.C.ddb_notfound(cursor) ~= 0
      end
   },

   __len = function (cursor)
      return tonumber(cursor:size())
   end
}

local DiscoDB = {
   __index = {
      keys = function (db)
         return ffi.gc(ffi.C.ddb_keys(db), ffi.C.ddb_free_cursor)
      end,

      values = function (db)
         return ffi.gc(ffi.C.ddb_values(db), ffi.C.ddb_free_cursor)
      end,

      unique_values = function (db)
         return ffi.gc(ffi.C.ddb_unique_values(db), ffi.C.ddb_free_cursor)
      end,

      getitem = function (db, k)
         return ffi.gc(ffi.C.ddb_getitem(db, k), ffi.C.ddb_free_cursor)
      end,

      get = function (db, key, default)
         local values = db:getitem(Entry.new(key))
         if values:notfound() then
            return default
         end
         return values
      end
   }
}

ffi.metatype('struct ddb_entry', Entry)
ffi.metatype('struct ddb_cursor', Cursor)
ffi.metatype('struct ddb', DiscoDB)

return {
   open = function (path)
      local file = assert(io.open(path, "r"))
      local fd = ffi.C.fileno(file)
      local db = ffi.gc(ffi.C.ddb_new(), ffi.C.ddb_free)
      if ffi.C.ddb_load(db, fd) > 0 then
         error("ddb_load")
      end
      return db
   end
}
