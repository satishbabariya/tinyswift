// TinySwift Runtime — unit tests for new stdlib functions.
// Compile: cc -o test_runtime test_runtime.c tinyswift_runtime.c -lm
// Run: ./test_runtime

#include "tinyswift_runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(cond, msg)                                                      \
  do {                                                                         \
    tests_run++;                                                               \
    if (cond) {                                                                \
      tests_passed++;                                                          \
    } else {                                                                   \
      fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__);                 \
    }                                                                          \
  } while (0)

// ── String tests ────────────────────────────────────────────────────────────

static void test_string_index_of(void) {
  ASSERT(__tinyswift_string_index_of("hello world", "world") == 6,
         "index_of 'world' in 'hello world'");
  ASSERT(__tinyswift_string_index_of("hello", "xyz") == -1,
         "index_of not found");
  ASSERT(__tinyswift_string_index_of("abcabc", "bc") == 1,
         "index_of first occurrence");
  ASSERT(__tinyswift_string_index_of("", "a") == -1, "index_of empty string");
}

static void test_string_replace(void) {
  char* r1 = __tinyswift_string_replace("hello world", "world", "Swift");
  ASSERT(strcmp(r1, "hello Swift") == 0, "replace single");
  free(r1);

  char* r2 = __tinyswift_string_replace("aaa", "a", "bb");
  ASSERT(strcmp(r2, "bbbbbb") == 0, "replace all occurrences");
  free(r2);

  char* r3 = __tinyswift_string_replace("hello", "xyz", "abc");
  ASSERT(strcmp(r3, "hello") == 0, "replace no match");
  free(r3);
}

static void test_string_substring(void) {
  char* s1 = __tinyswift_string_substring("hello world", 0, 5);
  ASSERT(strcmp(s1, "hello") == 0, "substring 0..5");
  free(s1);

  char* s2 = __tinyswift_string_substring("hello world", 6, 11);
  ASSERT(strcmp(s2, "world") == 0, "substring 6..11");
  free(s2);

  char* s3 = __tinyswift_string_substring("hello", 5, 5);
  ASSERT(strcmp(s3, "") == 0, "substring empty range");
  free(s3);
}

static void test_string_split(void) {
  int64_t count = 0;
  void* arr = __tinyswift_string_split("a,b,c", ",", &count);
  ASSERT(count == 3, "split count");
  char** p0 = (char**)__tinyswift_dynarray_get(arr, 0);
  char** p1 = (char**)__tinyswift_dynarray_get(arr, 1);
  char** p2 = (char**)__tinyswift_dynarray_get(arr, 2);
  ASSERT(p0 && strcmp(*p0, "a") == 0, "split[0]");
  ASSERT(p1 && strcmp(*p1, "b") == 0, "split[1]");
  ASSERT(p2 && strcmp(*p2, "c") == 0, "split[2]");
  // Cleanup.
  for (int64_t i = 0; i < count; i++) {
    char** pp = (char**)__tinyswift_dynarray_get(arr, i);
    if (pp) free(*pp);
  }
  __tinyswift_dynarray_destroy(arr);
}

static void test_string_conversions(void) {
  char* ds = __tinyswift_double_to_string(3.14);
  ASSERT(ds != NULL && strstr(ds, "3.14") != NULL, "double_to_string 3.14");
  free(ds);

  int64_t succ = 0;
  int64_t iv = __tinyswift_string_to_int("42", &succ);
  ASSERT(iv == 42 && succ == 1, "string_to_int '42'");

  succ = 0;
  __tinyswift_string_to_int("abc", &succ);
  ASSERT(succ == 0, "string_to_int 'abc' fails");

  succ = 0;
  double dv = __tinyswift_string_to_double("2.718", &succ);
  ASSERT(succ == 1 && fabs(dv - 2.718) < 0.001, "string_to_double '2.718'");

  succ = 0;
  __tinyswift_string_to_double("not_a_number", &succ);
  ASSERT(succ == 0, "string_to_double 'not_a_number' fails");
}

static void test_string_repeated(void) {
  char* r1 = __tinyswift_string_repeated("ab", 3);
  ASSERT(strcmp(r1, "ababab") == 0, "repeated 'ab' 3 times");
  free(r1);

  char* r2 = __tinyswift_string_repeated("x", 0);
  ASSERT(strcmp(r2, "") == 0, "repeated 0 times");
  free(r2);
}

// ── Math tests ──────────────────────────────────────────────────────────────

static void test_math(void) {
  ASSERT(fabs(__tinyswift_sqrt(4.0) - 2.0) < 1e-10, "sqrt(4)");
  ASSERT(fabs(__tinyswift_pow(2.0, 10.0) - 1024.0) < 1e-10, "pow(2,10)");
  ASSERT(fabs(__tinyswift_sin(0.0)) < 1e-10, "sin(0)");
  ASSERT(fabs(__tinyswift_cos(0.0) - 1.0) < 1e-10, "cos(0)");
  ASSERT(fabs(__tinyswift_floor(3.7) - 3.0) < 1e-10, "floor(3.7)");
  ASSERT(fabs(__tinyswift_ceil(3.2) - 4.0) < 1e-10, "ceil(3.2)");
  ASSERT(fabs(__tinyswift_round(3.5) - 4.0) < 1e-10, "round(3.5)");
  ASSERT(fabs(__tinyswift_fabs(-5.0) - 5.0) < 1e-10, "fabs(-5)");
  ASSERT(fabs(__tinyswift_fmod(10.0, 3.0) - 1.0) < 1e-10, "fmod(10,3)");
  ASSERT(fabs(__tinyswift_log(1.0)) < 1e-10, "log(1)");
  ASSERT(fabs(__tinyswift_log2(8.0) - 3.0) < 1e-10, "log2(8)");
  ASSERT(fabs(__tinyswift_log10(100.0) - 2.0) < 1e-10, "log10(100)");
  ASSERT(fabs(__tinyswift_tan(0.0)) < 1e-10, "tan(0)");
  ASSERT(fabs(__tinyswift_atan2(1.0, 1.0) - 0.7853981633974483) < 1e-10,
         "atan2(1,1)");
}

// ── Time tests ──────────────────────────────────────────────────────────────

static void test_time(void) {
  int64_t t1 = __tinyswift_clock_now();
  ASSERT(t1 > 0, "clock_now returns positive");

  int64_t m1 = __tinyswift_clock_monotonic();
  int64_t m2 = __tinyswift_clock_monotonic();
  ASSERT(m2 >= m1, "clock_monotonic is non-decreasing");
}

// ── DynArray tests ──────────────────────────────────────────────────────────

static void test_dynarray_insert_remove(void) {
  void* arr = __tinyswift_dynarray_create();

  int64_t v1 = 10, v2 = 20, v3 = 30;
  __tinyswift_dynarray_append(arr, &v1);
  __tinyswift_dynarray_append(arr, &v3);
  // Insert v2 at index 1: [10, 20, 30]
  __tinyswift_dynarray_insert(arr, 1, &v2);
  ASSERT(__tinyswift_dynarray_count(arr) == 3, "insert count");
  ASSERT(__tinyswift_dynarray_get_int(arr, 0) == 10, "insert[0]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 1) == 20, "insert[1]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 2) == 30, "insert[2]");

  // Remove at index 1: [10, 30]
  __tinyswift_dynarray_remove_at(arr, 1);
  ASSERT(__tinyswift_dynarray_count(arr) == 2, "remove_at count");
  ASSERT(__tinyswift_dynarray_get_int(arr, 0) == 10, "remove_at[0]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 1) == 30, "remove_at[1]");

  __tinyswift_dynarray_destroy(arr);
}

static void test_dynarray_clear_capacity(void) {
  void* arr = __tinyswift_dynarray_create();
  int64_t v = 42;
  for (int i = 0; i < 10; i++) {
    __tinyswift_dynarray_append(arr, &v);
  }
  ASSERT(__tinyswift_dynarray_count(arr) == 10, "pre-clear count");
  ASSERT(__tinyswift_dynarray_capacity(arr) >= 10, "capacity >= count");

  __tinyswift_dynarray_clear(arr);
  ASSERT(__tinyswift_dynarray_count(arr) == 0, "post-clear count");

  __tinyswift_dynarray_destroy(arr);
}

static int int_compare(const void* a, const void* b) {
  int64_t va, vb;
  memcpy(&va, a, sizeof(int64_t));
  memcpy(&vb, b, sizeof(int64_t));
  return (va > vb) - (va < vb);
}

static void test_dynarray_sort(void) {
  void* arr = __tinyswift_dynarray_create();
  int64_t vals[] = {30, 10, 50, 20, 40};
  for (int i = 0; i < 5; i++) {
    __tinyswift_dynarray_append(arr, &vals[i]);
  }
  __tinyswift_dynarray_sort(arr, int_compare);
  ASSERT(__tinyswift_dynarray_get_int(arr, 0) == 10, "sort[0]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 1) == 20, "sort[1]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 2) == 30, "sort[2]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 3) == 40, "sort[3]");
  ASSERT(__tinyswift_dynarray_get_int(arr, 4) == 50, "sort[4]");

  __tinyswift_dynarray_destroy(arr);
}

// ── Filesystem tests ────────────────────────────────────────────────────────

static void test_fs(void) {
  ASSERT(__tinyswift_getpid() > 0, "getpid > 0");
  // is_file on a known file
  ASSERT(__tinyswift_fs_is_file("/dev/null") == 0, "/dev/null is not regular");
}

// ── Memory tests ────────────────────────────────────────────────────────────

static void test_memory(void) {
  void* p = __tinyswift_raw_alloc(64, 16);
  ASSERT(p != NULL, "raw_alloc returns non-null");
  ASSERT(((size_t)p % 16) == 0, "raw_alloc aligned to 16");
  __tinyswift_memset(p, 0xAB, 64);
  ASSERT(__tinyswift_memcmp(p, p, 64) == 0, "memcmp same");
  __tinyswift_raw_dealloc(p);
}

// ── I/O tests ───────────────────────────────────────────────────────────────

static void test_io(void) {
  // Just verify they don't crash.
  __tinyswift_print_double(3.14);
  __tinyswift_print_bool(1);
  __tinyswift_print_bool(0);
  __tinyswift_print_newline();
  tests_run++;
  tests_passed++;
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(void) {
  test_string_index_of();
  test_string_replace();
  test_string_substring();
  test_string_split();
  test_string_conversions();
  test_string_repeated();
  test_math();
  test_time();
  test_dynarray_insert_remove();
  test_dynarray_clear_capacity();
  test_dynarray_sort();
  test_fs();
  test_memory();
  test_io();

  printf("\n%d/%d tests passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
