// TinySwift Runtime — core runtime library (M82).
// Link with every compiled TinySwift binary.
//
// Object layout (for ARC):
//   [TinySwiftHeapHeader: {refcount: int32_t, flags: int32_t}]  <- 8 bytes, hidden
//   [field_0: T0]  <- returned pointer points HERE
//   [field_1: T1]
//   ...

#include "tinyswift_runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ═══════════════════════════════════════════════════════════════════════════════
// ARC (M78/M79)
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
  int32_t refcount;
  int32_t flags;
} TinySwiftHeapHeader;

void* __tinyswift_alloc(int64_t payload_size) {
  void* raw = malloc(sizeof(TinySwiftHeapHeader) + (size_t)payload_size);
  if (!raw) return (void*)0;
  TinySwiftHeapHeader* header = (TinySwiftHeapHeader*)raw;
  header->refcount = 1;
  header->flags = 0;
  return (void*)((char*)raw + sizeof(TinySwiftHeapHeader));
}

void __tinyswift_retain(void* obj) {
  if (!obj) return;
  TinySwiftHeapHeader* header =
      (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));
  header->refcount++;
}

void __tinyswift_release(void* obj, void (*deinit_fn)(void*)) {
  if (!obj) return;
  TinySwiftHeapHeader* header =
      (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));
  header->refcount--;
  if (header->refcount <= 0) {
    if (deinit_fn) {
      deinit_fn(obj);
    }
    free(header);
  }
}

int64_t __tinyswift_is_unique(void* obj) {
  if (!obj) return 0;
  TinySwiftHeapHeader* header =
      (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));
  return header->refcount == 1 ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// I/O (M44)
// ═══════════════════════════════════════════════════════════════════════════════

void __tinyswift_print_int(int64_t value) {
  printf("%lld\n", (long long)value);
}

void __tinyswift_print_string(const char* str) {
  if (str) {
    printf("%s\n", str);
  } else {
    printf("(null)\n");
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// File I/O (M92)
// ═══════════════════════════════════════════════════════════════════════════════

char* __tinyswift_readline(void) {
  char buf[4096];
  if (!fgets(buf, sizeof(buf), stdin)) {
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  // Strip trailing newline.
  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
  if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
  char* result = (char*)malloc(len + 1);
  if (!result) return NULL;
  memcpy(result, buf, len + 1);
  return result;
}

char* __tinyswift_file_read_all(const char* path) {
  if (!path) goto fail;
  FILE* f = fopen(path, "rb");
  if (!f) goto fail;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) { fclose(f); goto fail; }
  char* buf = (char*)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); goto fail; }
  size_t nread = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[nread] = '\0';
  return buf;
fail:;
  char* empty = (char*)malloc(1);
  if (empty) empty[0] = '\0';
  return empty;
}

int64_t __tinyswift_file_write_all(const char* path, const char* data) {
  if (!path || !data) return 0;
  FILE* f = fopen(path, "wb");
  if (!f) return 0;
  size_t len = strlen(data);
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len ? 1 : 0;
}

int64_t __tinyswift_file_append_all(const char* path, const char* data) {
  if (!path || !data) return 0;
  FILE* f = fopen(path, "ab");
  if (!f) return 0;
  size_t len = strlen(data);
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len ? 1 : 0;
}

int64_t __tinyswift_file_exists(const char* path) {
  if (!path) return 0;
  struct stat st;
  return stat(path, &st) == 0 ? 1 : 0;
}

int64_t __tinyswift_file_remove(const char* path) {
  if (!path) return 0;
  return remove(path) == 0 ? 1 : 0;
}

char* __tinyswift_file_getcwd(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) {
    size_t len = strlen(buf);
    char* result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, buf, len + 1);
    return result;
  }
  char* empty = (char*)malloc(1);
  if (empty) empty[0] = '\0';
  return empty;
}

// ═══════════════════════════════════════════════════════════════════════════════
// String operations (M38, M45, M48, M49, M61)
// ═══════════════════════════════════════════════════════════════════════════════

char* __tinyswift_string_concat(const char* a, const char* b) {
  if (!a && !b) return NULL;
  size_t la = a ? strlen(a) : 0;
  size_t lb = b ? strlen(b) : 0;
  char* result = (char*)malloc(la + lb + 1);
  if (!result) return NULL;
  if (la) memcpy(result, a, la);
  if (lb) memcpy(result + la, b, lb);
  result[la + lb] = '\0';
  return result;
}

int64_t __tinyswift_string_len(const char* str) {
  if (!str) return 0;
  return (int64_t)strlen(str);
}

int64_t __tinyswift_string_eq(const char* a, const char* b) {
  if (a == b) return 1;
  if (!a || !b) return 0;
  return strcmp(a, b) == 0 ? 1 : 0;
}

char* __tinyswift_int_to_string(int64_t value) {
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "%lld", (long long)value);
  char* result = (char*)malloc((size_t)len + 1);
  if (!result) return NULL;
  memcpy(result, buf, (size_t)len + 1);
  return result;
}

char* __tinyswift_string_uppercased(const char* str) {
  if (!str) return NULL;
  size_t len = strlen(str);
  char* result = (char*)malloc(len + 1);
  if (!result) return NULL;
  for (size_t i = 0; i < len; ++i) {
    result[i] = (char)toupper((unsigned char)str[i]);
  }
  result[len] = '\0';
  return result;
}

char* __tinyswift_string_lowercased(const char* str) {
  if (!str) return NULL;
  size_t len = strlen(str);
  char* result = (char*)malloc(len + 1);
  if (!result) return NULL;
  for (size_t i = 0; i < len; ++i) {
    result[i] = (char)tolower((unsigned char)str[i]);
  }
  result[len] = '\0';
  return result;
}

char* __tinyswift_string_trimmed(const char* str) {
  if (!str) return NULL;
  // Skip leading whitespace.
  const char* start = str;
  while (*start && isspace((unsigned char)*start)) ++start;
  // Find trailing whitespace.
  const char* end = str + strlen(str);
  while (end > start && isspace((unsigned char)*(end - 1))) --end;
  size_t len = (size_t)(end - start);
  char* result = (char*)malloc(len + 1);
  if (!result) return NULL;
  memcpy(result, start, len);
  result[len] = '\0';
  return result;
}

int64_t __tinyswift_string_has_prefix(const char* str, const char* prefix) {
  if (!str || !prefix) return 0;
  size_t plen = strlen(prefix);
  return strncmp(str, prefix, plen) == 0 ? 1 : 0;
}

int64_t __tinyswift_string_has_suffix(const char* str, const char* suffix) {
  if (!str || !suffix) return 0;
  size_t slen = strlen(str);
  size_t xlen = strlen(suffix);
  if (xlen > slen) return 0;
  return strcmp(str + slen - xlen, suffix) == 0 ? 1 : 0;
}

int64_t __tinyswift_string_contains(const char* str, const char* substr) {
  if (!str || !substr) return 0;
  return strstr(str, substr) != NULL ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Dynamic array (M65 base, M90 generic type-erased)
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
  void* data;         // raw byte buffer
  int64_t count;
  int64_t capacity;
  int64_t elem_size;  // bytes per element
} TinySwiftDynArray;

void* __tinyswift_dynarray_create_generic(int64_t elem_size) {
  TinySwiftDynArray* arr = (TinySwiftDynArray*)malloc(sizeof(TinySwiftDynArray));
  if (!arr) return NULL;
  arr->data = NULL;
  arr->count = 0;
  arr->capacity = 0;
  arr->elem_size = elem_size > 0 ? elem_size : 8;
  return (void*)arr;
}

void __tinyswift_dynarray_append(void* handle, const void* elem) {
  if (!handle || !elem) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (arr->count >= arr->capacity) {
    int64_t new_cap = arr->capacity == 0 ? 8 : arr->capacity * 2;
    void* new_data = realloc(arr->data, (size_t)new_cap * (size_t)arr->elem_size);
    if (!new_data) return;
    arr->data = new_data;
    arr->capacity = new_cap;
  }
  memcpy((char*)arr->data + arr->count * arr->elem_size, elem,
         (size_t)arr->elem_size);
  arr->count++;
}

void* __tinyswift_dynarray_get(void* handle, int64_t index) {
  if (!handle) return NULL;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (index < 0 || index >= arr->count) return NULL;
  return (char*)arr->data + index * arr->elem_size;
}

void __tinyswift_dynarray_set(void* handle, int64_t index, const void* elem) {
  if (!handle || !elem) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (index < 0 || index >= arr->count) return;
  memcpy((char*)arr->data + index * arr->elem_size, elem,
         (size_t)arr->elem_size);
}

int64_t __tinyswift_dynarray_count(void* handle) {
  if (!handle) return 0;
  return ((TinySwiftDynArray*)handle)->count;
}

void __tinyswift_dynarray_remove_last(void* handle) {
  if (!handle) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (arr->count > 0) arr->count--;
}

void __tinyswift_dynarray_destroy(void* handle) {
  if (!handle) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  free(arr->data);
  free(arr);
}

// M65 compatibility wrappers (call the generic versions).
void* __tinyswift_dynarray_create(void) {
  return __tinyswift_dynarray_create_generic(sizeof(int64_t));
}

void __tinyswift_dynarray_append_int(void* handle, int64_t value) {
  __tinyswift_dynarray_append(handle, &value);
}

int64_t __tinyswift_dynarray_get_int(void* handle, int64_t index) {
  void* ptr = __tinyswift_dynarray_get(handle, index);
  if (!ptr) return 0;
  int64_t result;
  memcpy(&result, ptr, sizeof(int64_t));
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Dictionary (M42) — simplified linear-scan string→int map
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
  char** keys;
  int64_t* values;
  int64_t count;
  int64_t capacity;
} TinySwiftDict;

void* __tinyswift_dict_create(int64_t count, const char** keys,
                               int64_t* values) {
  TinySwiftDict* dict = (TinySwiftDict*)malloc(sizeof(TinySwiftDict));
  if (!dict) return NULL;
  dict->count = count;
  dict->capacity = count > 0 ? count : 4;
  dict->keys = (char**)malloc((size_t)dict->capacity * sizeof(char*));
  dict->values = (int64_t*)malloc((size_t)dict->capacity * sizeof(int64_t));
  if (!dict->keys || !dict->values) {
    free(dict->keys);
    free(dict->values);
    free(dict);
    return NULL;
  }
  for (int64_t i = 0; i < count; ++i) {
    dict->keys[i] = keys ? strdup(keys[i]) : NULL;
    dict->values[i] = values ? values[i] : 0;
  }
  return (void*)dict;
}

TinySwiftDictResult __tinyswift_dict_get_str_int(void* handle,
                                                  const char* key) {
  TinySwiftDictResult result = {0, 0};
  if (!handle || !key) return result;
  TinySwiftDict* dict = (TinySwiftDict*)handle;
  for (int64_t i = 0; i < dict->count; ++i) {
    if (dict->keys[i] && strcmp(dict->keys[i], key) == 0) {
      result.found = 1;
      result.value = dict->values[i];
      return result;
    }
  }
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// M91: Generic Hash Map (type-erased, open addressing)
// ═══════════════════════════════════════════════════════════════════════════════

typedef int64_t (*TinySwiftEqFn)(const void*, const void*);

typedef struct {
  void* keys;         // raw byte buffer for keys
  void* values;       // raw byte buffer for values
  uint8_t* states;    // 0=empty, 1=occupied, 2=tombstone
  int64_t count;
  int64_t capacity;
  int64_t key_size;
  int64_t val_size;
  TinySwiftEqFn eq_fn;
} TinySwiftHashMap;

#define HASHMAP_LOAD_FACTOR 70  // percent

static void hashmap_rehash(TinySwiftHashMap* map) {
  int64_t old_cap = map->capacity;
  void* old_keys = map->keys;
  void* old_vals = map->values;
  uint8_t* old_states = map->states;

  int64_t new_cap = old_cap * 2;
  map->keys = calloc((size_t)new_cap, (size_t)map->key_size);
  map->values = calloc((size_t)new_cap, (size_t)map->val_size);
  map->states = (uint8_t*)calloc((size_t)new_cap, 1);
  map->capacity = new_cap;
  map->count = 0;

  for (int64_t i = 0; i < old_cap; ++i) {
    if (old_states[i] == 1) {
      const void* k = (const char*)old_keys + i * map->key_size;
      const void* v = (const char*)old_vals + i * map->val_size;
      // Compute hash by reading key bytes — use a simple FNV-like hash.
      uint64_t h = 14695981039346656037ULL;
      const uint8_t* kb = (const uint8_t*)k;
      for (int64_t b = 0; b < map->key_size; ++b) {
        h ^= kb[b];
        h *= 1099511628211ULL;
      }
      int64_t idx = (int64_t)(h % (uint64_t)new_cap);
      while (map->states[idx] == 1) {
        idx = (idx + 1) % new_cap;
      }
      memcpy((char*)map->keys + idx * map->key_size, k, (size_t)map->key_size);
      memcpy((char*)map->values + idx * map->val_size, v, (size_t)map->val_size);
      map->states[idx] = 1;
      map->count++;
    }
  }
  free(old_keys);
  free(old_vals);
  free(old_states);
}

void* __tinyswift_hashmap_create(int64_t key_size, int64_t val_size,
                                  TinySwiftEqFn eq_fn) {
  TinySwiftHashMap* map = (TinySwiftHashMap*)malloc(sizeof(TinySwiftHashMap));
  if (!map) return NULL;
  int64_t cap = 16;
  map->keys = calloc((size_t)cap, (size_t)key_size);
  map->values = calloc((size_t)cap, (size_t)val_size);
  map->states = (uint8_t*)calloc((size_t)cap, 1);
  map->count = 0;
  map->capacity = cap;
  map->key_size = key_size;
  map->val_size = val_size;
  map->eq_fn = eq_fn;
  return (void*)map;
}

void __tinyswift_hashmap_set(void* handle, const void* key_ptr,
                              int64_t key_hash, const void* val_ptr) {
  if (!handle || !key_ptr || !val_ptr) return;
  TinySwiftHashMap* map = (TinySwiftHashMap*)handle;

  // Check if rehash needed.
  if (map->count * 100 >= map->capacity * HASHMAP_LOAD_FACTOR) {
    hashmap_rehash(map);
  }

  int64_t idx = (int64_t)((uint64_t)key_hash % (uint64_t)map->capacity);
  int64_t first_tombstone = -1;
  for (int64_t i = 0; i < map->capacity; ++i) {
    int64_t slot = (idx + i) % map->capacity;
    if (map->states[slot] == 0) {
      // Empty slot — insert here (or at first tombstone).
      int64_t target = first_tombstone >= 0 ? first_tombstone : slot;
      memcpy((char*)map->keys + target * map->key_size, key_ptr,
             (size_t)map->key_size);
      memcpy((char*)map->values + target * map->val_size, val_ptr,
             (size_t)map->val_size);
      map->states[target] = 1;
      map->count++;
      return;
    }
    if (map->states[slot] == 2) {
      if (first_tombstone < 0) first_tombstone = slot;
      continue;
    }
    // Occupied — check if same key (update).
    const void* existing_key = (const char*)map->keys + slot * map->key_size;
    if (map->eq_fn(existing_key, key_ptr)) {
      memcpy((char*)map->values + slot * map->val_size, val_ptr,
             (size_t)map->val_size);
      return;
    }
  }
  // Shouldn't reach here if load factor is maintained.
  if (first_tombstone >= 0) {
    memcpy((char*)map->keys + first_tombstone * map->key_size, key_ptr,
           (size_t)map->key_size);
    memcpy((char*)map->values + first_tombstone * map->val_size, val_ptr,
           (size_t)map->val_size);
    map->states[first_tombstone] = 1;
    map->count++;
  }
}

int64_t __tinyswift_hashmap_get(void* handle, const void* key_ptr,
                                 int64_t key_hash, void* out_val_ptr) {
  if (!handle || !key_ptr) return 0;
  TinySwiftHashMap* map = (TinySwiftHashMap*)handle;
  int64_t idx = (int64_t)((uint64_t)key_hash % (uint64_t)map->capacity);
  for (int64_t i = 0; i < map->capacity; ++i) {
    int64_t slot = (idx + i) % map->capacity;
    if (map->states[slot] == 0) return 0;  // empty — not found
    if (map->states[slot] == 2) continue;   // tombstone — skip
    const void* existing_key = (const char*)map->keys + slot * map->key_size;
    if (map->eq_fn(existing_key, key_ptr)) {
      if (out_val_ptr) {
        memcpy(out_val_ptr, (const char*)map->values + slot * map->val_size,
               (size_t)map->val_size);
      }
      return 1;
    }
  }
  return 0;
}

int64_t __tinyswift_hashmap_count(void* handle) {
  if (!handle) return 0;
  TinySwiftHashMap* map = (TinySwiftHashMap*)handle;
  return map->count;
}

int64_t __tinyswift_hashmap_contains(void* handle, const void* key_ptr,
                                      int64_t key_hash) {
  if (!handle || !key_ptr) return 0;
  TinySwiftHashMap* map = (TinySwiftHashMap*)handle;
  int64_t idx = (int64_t)((uint64_t)key_hash % (uint64_t)map->capacity);
  for (int64_t i = 0; i < map->capacity; ++i) {
    int64_t slot = (idx + i) % map->capacity;
    if (map->states[slot] == 0) return 0;
    if (map->states[slot] == 2) continue;
    const void* existing_key = (const char*)map->keys + slot * map->key_size;
    if (map->eq_fn(existing_key, key_ptr)) return 1;
  }
  return 0;
}

void __tinyswift_hashmap_remove(void* handle, const void* key_ptr,
                                 int64_t key_hash) {
  if (!handle || !key_ptr) return;
  TinySwiftHashMap* map = (TinySwiftHashMap*)handle;
  int64_t idx = (int64_t)((uint64_t)key_hash % (uint64_t)map->capacity);
  for (int64_t i = 0; i < map->capacity; ++i) {
    int64_t slot = (idx + i) % map->capacity;
    if (map->states[slot] == 0) return;
    if (map->states[slot] == 2) continue;
    const void* existing_key = (const char*)map->keys + slot * map->key_size;
    if (map->eq_fn(existing_key, key_ptr)) {
      map->states[slot] = 2;  // tombstone
      map->count--;
      return;
    }
  }
}

void __tinyswift_hashmap_destroy(void* handle) {
  if (!handle) return;
  TinySwiftHashMap* map = (TinySwiftHashMap*)handle;
  free(map->keys);
  free(map->values);
  free(map->states);
  free(map);
}

// ═══════════════════════════════════════════════════════════════════════════════
// M91: Generic Hash Set (type-erased, open addressing)
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
  void* elems;        // raw byte buffer
  uint8_t* states;    // 0=empty, 1=occupied, 2=tombstone
  int64_t count;
  int64_t capacity;
  int64_t elem_size;
  TinySwiftEqFn eq_fn;
} TinySwiftHashSet;

static void hashset_rehash(TinySwiftHashSet* set) {
  int64_t old_cap = set->capacity;
  void* old_elems = set->elems;
  uint8_t* old_states = set->states;

  int64_t new_cap = old_cap * 2;
  set->elems = calloc((size_t)new_cap, (size_t)set->elem_size);
  set->states = (uint8_t*)calloc((size_t)new_cap, 1);
  set->capacity = new_cap;
  set->count = 0;

  for (int64_t i = 0; i < old_cap; ++i) {
    if (old_states[i] == 1) {
      const void* e = (const char*)old_elems + i * set->elem_size;
      uint64_t h = 14695981039346656037ULL;
      const uint8_t* eb = (const uint8_t*)e;
      for (int64_t b = 0; b < set->elem_size; ++b) {
        h ^= eb[b];
        h *= 1099511628211ULL;
      }
      int64_t idx = (int64_t)(h % (uint64_t)new_cap);
      while (set->states[idx] == 1) {
        idx = (idx + 1) % new_cap;
      }
      memcpy((char*)set->elems + idx * set->elem_size, e,
             (size_t)set->elem_size);
      set->states[idx] = 1;
      set->count++;
    }
  }
  free(old_elems);
  free(old_states);
}

void* __tinyswift_hashset_create(int64_t elem_size, TinySwiftEqFn eq_fn) {
  TinySwiftHashSet* set = (TinySwiftHashSet*)malloc(sizeof(TinySwiftHashSet));
  if (!set) return NULL;
  int64_t cap = 16;
  set->elems = calloc((size_t)cap, (size_t)elem_size);
  set->states = (uint8_t*)calloc((size_t)cap, 1);
  set->count = 0;
  set->capacity = cap;
  set->elem_size = elem_size;
  set->eq_fn = eq_fn;
  return (void*)set;
}

void __tinyswift_hashset_insert(void* handle, const void* elem_ptr,
                                 int64_t hash) {
  if (!handle || !elem_ptr) return;
  TinySwiftHashSet* set = (TinySwiftHashSet*)handle;
  if (set->count * 100 >= set->capacity * HASHMAP_LOAD_FACTOR) {
    hashset_rehash(set);
  }
  int64_t idx = (int64_t)((uint64_t)hash % (uint64_t)set->capacity);
  int64_t first_tombstone = -1;
  for (int64_t i = 0; i < set->capacity; ++i) {
    int64_t slot = (idx + i) % set->capacity;
    if (set->states[slot] == 0) {
      int64_t target = first_tombstone >= 0 ? first_tombstone : slot;
      memcpy((char*)set->elems + target * set->elem_size, elem_ptr,
             (size_t)set->elem_size);
      set->states[target] = 1;
      set->count++;
      return;
    }
    if (set->states[slot] == 2) {
      if (first_tombstone < 0) first_tombstone = slot;
      continue;
    }
    const void* existing = (const char*)set->elems + slot * set->elem_size;
    if (set->eq_fn(existing, elem_ptr)) return;  // already present
  }
  if (first_tombstone >= 0) {
    memcpy((char*)set->elems + first_tombstone * set->elem_size, elem_ptr,
           (size_t)set->elem_size);
    set->states[first_tombstone] = 1;
    set->count++;
  }
}

int64_t __tinyswift_hashset_contains(void* handle, const void* elem_ptr,
                                      int64_t hash) {
  if (!handle || !elem_ptr) return 0;
  TinySwiftHashSet* set = (TinySwiftHashSet*)handle;
  int64_t idx = (int64_t)((uint64_t)hash % (uint64_t)set->capacity);
  for (int64_t i = 0; i < set->capacity; ++i) {
    int64_t slot = (idx + i) % set->capacity;
    if (set->states[slot] == 0) return 0;
    if (set->states[slot] == 2) continue;
    const void* existing = (const char*)set->elems + slot * set->elem_size;
    if (set->eq_fn(existing, elem_ptr)) return 1;
  }
  return 0;
}

int64_t __tinyswift_hashset_count(void* handle) {
  if (!handle) return 0;
  return ((TinySwiftHashSet*)handle)->count;
}

void __tinyswift_hashset_remove(void* handle, const void* elem_ptr,
                                 int64_t hash) {
  if (!handle || !elem_ptr) return;
  TinySwiftHashSet* set = (TinySwiftHashSet*)handle;
  int64_t idx = (int64_t)((uint64_t)hash % (uint64_t)set->capacity);
  for (int64_t i = 0; i < set->capacity; ++i) {
    int64_t slot = (idx + i) % set->capacity;
    if (set->states[slot] == 0) return;
    if (set->states[slot] == 2) continue;
    const void* existing = (const char*)set->elems + slot * set->elem_size;
    if (set->eq_fn(existing, elem_ptr)) {
      set->states[slot] = 2;
      set->count--;
      return;
    }
  }
}

void __tinyswift_hashset_destroy(void* handle) {
  if (!handle) return;
  TinySwiftHashSet* set = (TinySwiftHashSet*)handle;
  free(set->elems);
  free(set->states);
  free(set);
}

// ═══════════════════════════════════════════════════════════════════════════════
// M91: Equality callbacks (passed as function pointers to hash map/set)
// ═══════════════════════════════════════════════════════════════════════════════

int64_t __tinyswift_eq_int(const void* a, const void* b) {
  return memcmp(a, b, 8) == 0 ? 1 : 0;
}

int64_t __tinyswift_eq_string(const void* a, const void* b) {
  const char* sa = *(const char**)a;
  const char* sb = *(const char**)b;
  if (sa == sb) return 1;
  if (!sa || !sb) return 0;
  return strcmp(sa, sb) == 0 ? 1 : 0;
}

int64_t __tinyswift_eq_bool(const void* a, const void* b) {
  return memcmp(a, b, 1) == 0 ? 1 : 0;
}

int64_t __tinyswift_eq_double(const void* a, const void* b) {
  return memcmp(a, b, 8) == 0 ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Error handling (M81 — thread-local error slot)
// ═══════════════════════════════════════════════════════════════════════════════

static _Thread_local int __tinyswift_has_error = 0;
static _Thread_local int64_t __tinyswift_error_value = 0;

void __tinyswift_error_set(int64_t error) {
  __tinyswift_has_error = 1;
  __tinyswift_error_value = error;
}

int64_t __tinyswift_error_get(void) {
  return __tinyswift_error_value;
}

int __tinyswift_error_check(void) {
  return __tinyswift_has_error;
}

void __tinyswift_error_clear(void) {
  __tinyswift_has_error = 0;
  __tinyswift_error_value = 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Prelude support (M88)
// ═══════════════════════════════════════════════════════════════════════════════

int64_t __tinyswift_int_abs(int64_t x) {
  return x < 0 ? -x : x;
}

int64_t __tinyswift_int_clamp(int64_t x, int64_t lo, int64_t hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

int64_t __tinyswift_int_hash(int64_t x) {
  // Simple hash: mix bits via multiply-shift.
  uint64_t h = (uint64_t)x;
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53ULL;
  h ^= h >> 33;
  return (int64_t)h;
}

double __tinyswift_double_abs(double x) {
  return x < 0.0 ? -x : x;
}

int64_t __tinyswift_double_hash(double x) {
  // Hash the raw bits of the double.
  union { double d; uint64_t u; } conv;
  conv.d = x;
  return __tinyswift_int_hash((int64_t)conv.u);
}

int64_t __tinyswift_string_hash(const char* str) {
  if (!str) return 0;
  // djb2 hash.
  uint64_t h = 5381;
  for (const char* p = str; *p; ++p) {
    h = ((h << 5) + h) + (unsigned char)*p;
  }
  return (int64_t)h;
}

int64_t __tinyswift_string_compare(const char* a, const char* b) {
  if (a == b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  return (int64_t)strcmp(a, b);
}

int64_t __tinyswift_bool_hash(int64_t x) {
  return x ? 1 : 0;
}

void __tinyswift_abort(const char* message) {
  if (message) {
    fprintf(stderr, "Fatal error: %s\n", message);
  } else {
    fprintf(stderr, "Fatal error\n");
  }
  abort();
}
