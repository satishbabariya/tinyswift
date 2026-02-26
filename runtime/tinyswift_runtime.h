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

// ── Dynamic array (M65) ────────────────────────────────────────────────────
void*   __tinyswift_dynarray_create(void);
void    __tinyswift_dynarray_append_int(void* arr, int64_t value);
int64_t __tinyswift_dynarray_count(void* arr);
int64_t __tinyswift_dynarray_get_int(void* arr, int64_t index);

// ── Dictionary (M42) ───────────────────────────────────────────────────────
typedef struct {
  int64_t found;
  int64_t value;
} TinySwiftDictResult;

void*             __tinyswift_dict_create(int64_t count, const char** keys,
                                          int64_t* values);
TinySwiftDictResult __tinyswift_dict_get_str_int(void* dict, const char* key);

// ── Error handling (M81) ───────────────────────────────────────────────────
void    __tinyswift_error_set(int64_t error);
int64_t __tinyswift_error_get(void);
int     __tinyswift_error_check(void);
void    __tinyswift_error_clear(void);

#ifdef __cplusplus
}
#endif

#endif // TINYSWIFT_RUNTIME_H
