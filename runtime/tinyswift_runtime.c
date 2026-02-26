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
// Dynamic array (M65)
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
  int64_t* data;
  int64_t count;
  int64_t capacity;
} TinySwiftDynArray;

void* __tinyswift_dynarray_create(void) {
  TinySwiftDynArray* arr = (TinySwiftDynArray*)malloc(sizeof(TinySwiftDynArray));
  if (!arr) return NULL;
  arr->data = NULL;
  arr->count = 0;
  arr->capacity = 0;
  return (void*)arr;
}

void __tinyswift_dynarray_append_int(void* handle, int64_t value) {
  if (!handle) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (arr->count >= arr->capacity) {
    int64_t new_cap = arr->capacity == 0 ? 8 : arr->capacity * 2;
    int64_t* new_data =
        (int64_t*)realloc(arr->data, (size_t)new_cap * sizeof(int64_t));
    if (!new_data) return;
    arr->data = new_data;
    arr->capacity = new_cap;
  }
  arr->data[arr->count] = value;
  arr->count++;
}

int64_t __tinyswift_dynarray_count(void* handle) {
  if (!handle) return 0;
  return ((TinySwiftDynArray*)handle)->count;
}

int64_t __tinyswift_dynarray_get_int(void* handle, int64_t index) {
  if (!handle) return 0;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (index < 0 || index >= arr->count) return 0;
  return arr->data[index];
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
