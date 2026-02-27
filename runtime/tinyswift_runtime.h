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

// ── Cycle Collection (M97) ─────────────────────────────────────────────────
// Release for cycle-capable types: if refcount > 0 after decrement, registers
// the object as a cycle candidate for later collection.
void    __tinyswift_release_cycle_candidate(void* obj, void (*deinit_fn)(void*));
// Run trial-deletion cycle collector. Frees all unreachable cycles.
void    __tinyswift_collect_cycles(void);

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
int64_t __tinyswift_string_index_of(const char* s, const char* sub);
char*   __tinyswift_string_replace(const char* s, const char* target,
                                    const char* replacement);
char*   __tinyswift_string_substring(const char* s, int64_t from, int64_t to);
void*   __tinyswift_string_split(const char* s, const char* sep,
                                  int64_t* out_count);
char*   __tinyswift_double_to_string(double d);
int64_t __tinyswift_string_to_int(const char* s, int64_t* out_success);
double  __tinyswift_string_to_double(const char* s, int64_t* out_success);
char*   __tinyswift_string_repeated(const char* s, int64_t count);

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

void    __tinyswift_dynarray_insert(void* arr, int64_t index, const void* elem);
void    __tinyswift_dynarray_remove_at(void* arr, int64_t index);
void    __tinyswift_dynarray_clear(void* arr);
int64_t __tinyswift_dynarray_capacity(void* arr);
void    __tinyswift_dynarray_sort(void* arr,
                                   int (*compare_fn)(const void*, const void*));

// ── I/O extensions ──────────────────────────────────────────────────────────
void __tinyswift_print_double(double d);
void __tinyswift_print_bool(int64_t b);
void __tinyswift_print_newline(void);

// ── Math (libm wrappers) ────────────────────────────────────────────────────
double __tinyswift_sqrt(double x);
double __tinyswift_pow(double base, double exp);
double __tinyswift_log(double x);
double __tinyswift_log2(double x);
double __tinyswift_log10(double x);
double __tinyswift_floor(double x);
double __tinyswift_ceil(double x);
double __tinyswift_round(double x);
double __tinyswift_sin(double x);
double __tinyswift_cos(double x);
double __tinyswift_tan(double x);
double __tinyswift_atan2(double y, double x);
double __tinyswift_fabs(double x);
double __tinyswift_fmod(double x, double y);

// ── Time ────────────────────────────────────────────────────────────────────
int64_t __tinyswift_clock_now(void);
int64_t __tinyswift_clock_monotonic(void);

// ── Filesystem/Process additions ────────────────────────────────────────────
int64_t __tinyswift_fs_is_file(const char* path);
int64_t __tinyswift_fs_rename(const char* from, const char* to);
int64_t __tinyswift_getpid(void);
void    __tinyswift_env_unset(const char* key);

// ── Memory utilities ────────────────────────────────────────────────────────
void*   __tinyswift_raw_alloc(int64_t size, int64_t alignment);
void    __tinyswift_raw_dealloc(void* ptr);
void    __tinyswift_memcpy(void* dst, const void* src, int64_t size);
void    __tinyswift_memset(void* dst, int64_t value, int64_t size);
int64_t __tinyswift_memcmp(const void* a, const void* b, int64_t size);

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

// ── OS: Process & Environment (M93) ────────────────────────────────────────
void    __tinyswift_exit(int64_t code);
void*   __tinyswift_get_args(void);              // returns dynarray of strings
char*   __tinyswift_env_get(const char* key);
int64_t __tinyswift_env_set(const char* key, const char* val);

// ── OS: FileSystem extensions (M93) ────────────────────────────────────────
int64_t __tinyswift_fs_mkdir(const char* path);
void*   __tinyswift_fs_listdir(const char* path); // returns dynarray of strings
int64_t __tinyswift_fs_is_dir(const char* path);
int64_t __tinyswift_fs_copy(const char* src, const char* dst);

// ── Networking: TCP Sockets (M94) ──────────────────────────────────────────
int64_t __tinyswift_tcp_connect(const char* host, int64_t port);
int64_t __tinyswift_tcp_listen(int64_t port);
int64_t __tinyswift_tcp_accept(int64_t fd);
char*   __tinyswift_tcp_read(int64_t fd, int64_t maxlen);
int64_t __tinyswift_tcp_write(int64_t fd, const char* data);
int64_t __tinyswift_tcp_close(int64_t fd);

// ── Async Runtime (M100-M102) ──────────────────────────────────────────────
// blockOn: synchronously runs an async function to completion.
// In M100, async functions run synchronously, so this is a simple wrapper.
int64_t __tinyswift_block_on(void* frame, void* (*resume_fn)(void*));

// Event loop task submission (M101-M102).
void    __tinyswift_async_submit(void* frame, void* (*resume_fn)(void*));
void    __tinyswift_run_event_loop(void);

// I/O polling and async registration (M102).
void    __tinyswift_io_poll(int64_t timeout_ms);
void    __tinyswift_io_register_read(int64_t fd, void* frame,
                                      void* (*resume_fn)(void*));
void    __tinyswift_io_register_write(int64_t fd, void* frame,
                                       void* (*resume_fn)(void*));

// Timer support (M102).
void    __tinyswift_timer_create(int64_t ms, void* frame,
                                  void* (*resume_fn)(void*));

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

// ── Test Runner Helper (M117) ─────────────────────────────────────────────
// Runs a test function, catching assertion failures and crashes.
// Returns 1 on success, 0 on failure.
int     __tinyswift_run_test(void (*test_fn)(void));

#ifdef __cplusplus
}
#endif

#endif // TINYSWIFT_RUNTIME_H
