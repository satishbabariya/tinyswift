// TinySwift Runtime — public API header.
// All functions are prefixed with __tinyswift_ to avoid symbol collisions.

#ifndef TINYSWIFT_RUNTIME_H
#define TINYSWIFT_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── ARC (M78/M79) ──────────────────────────────────────────────────────────
void*   __tinyswift_alloc(int64_t payload_size);
void    __tinyswift_retain(void* obj);
void    __tinyswift_release(void* obj, void (*deinit_fn)(void*));
int64_t __tinyswift_is_unique(void* obj);

// ── I/O (M44) ───────────────────────────────────────────────────────────────
void __tinyswift_print_int(int64_t value);
void __tinyswift_print_string(const char* str);

// ── File I/O (M92) ────────────────────────────────────────────────────────
char*   __tinyswift_readline(void);
char*   __tinyswift_file_read_all(const char* path);
int64_t __tinyswift_file_write_all(const char* path, const char* data);
int64_t __tinyswift_file_append_all(const char* path, const char* data);
int64_t __tinyswift_file_exists(const char* path);
int64_t __tinyswift_file_remove(const char* path);
char*   __tinyswift_file_getcwd(void);

// ── String operations (M38, M45, M48, M49, M61) ────────────────────────────
char*   __tinyswift_string_concat(const char* a, const char* b);
int64_t __tinyswift_string_len(const char* str);
int64_t __tinyswift_string_eq(const char* a, const char* b);
char*   __tinyswift_int_to_string(int64_t value);
char*   __tinyswift_string_uppercased(const char* str);
char*   __tinyswift_string_lowercased(const char* str);
char*   __tinyswift_string_trimmed(const char* str);
int64_t __tinyswift_string_has_prefix(const char* str, const char* prefix);
int64_t __tinyswift_string_has_suffix(const char* str, const char* suffix);
int64_t __tinyswift_string_contains(const char* str, const char* substr);

// ── Dynamic array (M65 base, M90 generic) ─────────────────────────────────
void*   __tinyswift_dynarray_create_generic(int64_t elem_size);
void    __tinyswift_dynarray_append(void* arr, const void* elem);
void*   __tinyswift_dynarray_get(void* arr, int64_t index);
void    __tinyswift_dynarray_set(void* arr, int64_t index, const void* elem);
int64_t __tinyswift_dynarray_count(void* arr);
void    __tinyswift_dynarray_remove_last(void* arr);
void    __tinyswift_dynarray_destroy(void* arr);
// M65 compatibility wrappers:
void*   __tinyswift_dynarray_create(void);
void    __tinyswift_dynarray_append_int(void* arr, int64_t value);
int64_t __tinyswift_dynarray_get_int(void* arr, int64_t index);

// ── Dictionary (M42 — legacy compatibility) ──────────────────────────────
typedef struct {
  int64_t found;
  int64_t value;
} TinySwiftDictResult;

void*             __tinyswift_dict_create(int64_t count, const char** keys,
                                          int64_t* values);
TinySwiftDictResult __tinyswift_dict_get_str_int(void* dict, const char* key);

// ── Generic Hash Map (M91) ───────────────────────────────────────────────
typedef int64_t (*TinySwiftEqFn)(const void*, const void*);

void*   __tinyswift_hashmap_create(int64_t key_size, int64_t val_size,
                                    TinySwiftEqFn eq_fn);
void    __tinyswift_hashmap_set(void* map, const void* key_ptr,
                                 int64_t key_hash, const void* val_ptr);
int64_t __tinyswift_hashmap_get(void* map, const void* key_ptr,
                                 int64_t key_hash, void* out_val_ptr);
int64_t __tinyswift_hashmap_count(void* map);
int64_t __tinyswift_hashmap_contains(void* map, const void* key_ptr,
                                      int64_t key_hash);
void    __tinyswift_hashmap_remove(void* map, const void* key_ptr,
                                    int64_t key_hash);
void    __tinyswift_hashmap_destroy(void* map);

// ── Generic Hash Set (M91) ───────────────────────────────────────────────
void*   __tinyswift_hashset_create(int64_t elem_size, TinySwiftEqFn eq_fn);
void    __tinyswift_hashset_insert(void* set, const void* elem_ptr,
                                    int64_t hash);
int64_t __tinyswift_hashset_contains(void* set, const void* elem_ptr,
                                      int64_t hash);
int64_t __tinyswift_hashset_count(void* set);
void    __tinyswift_hashset_remove(void* set, const void* elem_ptr,
                                    int64_t hash);
void    __tinyswift_hashset_destroy(void* set);

// ── Equality callbacks (M91) ─────────────────────────────────────────────
int64_t __tinyswift_eq_int(const void* a, const void* b);
int64_t __tinyswift_eq_string(const void* a, const void* b);
int64_t __tinyswift_eq_bool(const void* a, const void* b);
int64_t __tinyswift_eq_double(const void* a, const void* b);

// ── Error handling (M81) ───────────────────────────────────────────────────
void    __tinyswift_error_set(int64_t error);
int64_t __tinyswift_error_get(void);
int     __tinyswift_error_check(void);
void    __tinyswift_error_clear(void);

// ── Prelude support (M88) ──────────────────────────────────────────────────
int64_t __tinyswift_int_abs(int64_t x);
int64_t __tinyswift_int_clamp(int64_t x, int64_t lo, int64_t hi);
int64_t __tinyswift_int_hash(int64_t x);
double  __tinyswift_double_abs(double x);
int64_t __tinyswift_double_hash(double x);
int64_t __tinyswift_string_hash(const char* str);
int64_t __tinyswift_string_compare(const char* a, const char* b);
int64_t __tinyswift_bool_hash(int64_t x);
void    __tinyswift_abort(const char* message);

#ifdef __cplusplus
}
#endif

#endif // TINYSWIFT_RUNTIME_H
