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
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// Networking headers (M94)
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

// macOS: _NSGetArgc/_NSGetArgv (M93)
#ifdef __APPLE__
#include <crt_externs.h>
#endif

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
// Cycle Collection (M97)
// ═══════════════════════════════════════════════════════════════════════════════

// Candidate list — objects whose refcount was decremented to non-zero.
// These might be part of a reference cycle.
#define CYCLE_CANDIDATES_MAX 1024

typedef struct {
  void* obj;
  void (*deinit_fn)(void*);
} CycleCandidate;

static _Thread_local CycleCandidate cycle_candidates[CYCLE_CANDIDATES_MAX];
static _Thread_local int64_t cycle_candidate_count = 0;
static int cycle_atexit_registered = 0;

void __tinyswift_release_cycle_candidate(void* obj, void (*deinit_fn)(void*)) {
  if (!obj) return;
  // Register atexit handler on first use to collect remaining cycles.
  if (!cycle_atexit_registered) {
    cycle_atexit_registered = 1;
    atexit(__tinyswift_collect_cycles);
  }
  TinySwiftHeapHeader* header =
      (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));
  header->refcount--;
  if (header->refcount <= 0) {
    // Refcount hit zero — deallocate normally (no cycle).
    if (deinit_fn) {
      deinit_fn(obj);
    }
    free(header);
  } else {
    // Refcount > 0 — might be part of a cycle. Register as candidate.
    if (cycle_candidate_count < CYCLE_CANDIDATES_MAX) {
      cycle_candidates[cycle_candidate_count].obj = obj;
      cycle_candidates[cycle_candidate_count].deinit_fn = deinit_fn;
      cycle_candidate_count++;
    }
    // If the candidate list is full, we just skip — the cycle will leak.
    // A production implementation would grow the list or collect immediately.
  }
}

void __tinyswift_collect_cycles(void) {
  // Trial deletion cycle collector:
  // 1. For each candidate, check if refcount is still > 0.
  // 2. If refcount is 1 and the object is a candidate, it's likely
  //    only held alive by the cycle. We do a simple heuristic: if the
  //    refcount equals the number of times this object appears as a
  //    candidate, it's unreachable.
  //
  // This is a simplified collector that handles the common case of
  // simple two-object cycles. A full implementation would trace object
  // graphs.

  if (cycle_candidate_count == 0) return;

  // Deduplicate candidates and count references.
  // Simple O(n^2) for small candidate lists.
  for (int64_t i = 0; i < cycle_candidate_count; ++i) {
    void* obj = cycle_candidates[i].obj;
    if (!obj) continue;

    TinySwiftHeapHeader* header =
        (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));

    // Skip already-freed objects (refcount <= 0).
    if (header->refcount <= 0) {
      cycle_candidates[i].obj = NULL;
      continue;
    }

    // Count how many candidates point to this same object.
    int64_t candidate_refs = 0;
    for (int64_t j = 0; j < cycle_candidate_count; ++j) {
      if (cycle_candidates[j].obj == obj) {
        candidate_refs++;
      }
    }

    // If the refcount equals the number of candidate references,
    // the object is only kept alive by other candidates (cycle).
    if (header->refcount <= (int32_t)candidate_refs) {
      // Force-free this object.
      void (*deinit_fn)(void*) = cycle_candidates[i].deinit_fn;
      header->refcount = 0;
      if (deinit_fn) {
        deinit_fn(obj);
      }
      free(header);

      // Null out all candidate entries for this object.
      for (int64_t j = 0; j < cycle_candidate_count; ++j) {
        if (cycle_candidates[j].obj == obj) {
          cycle_candidates[j].obj = NULL;
        }
      }
    }
  }

  // Clear the candidate list.
  cycle_candidate_count = 0;
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
// OS: Process & Environment (M93)
// ═══════════════════════════════════════════════════════════════════════════════

void __tinyswift_exit(int64_t code) {
  exit((int)code);
}

void* __tinyswift_get_args(void) {
  // Create a dynarray of strings (char* elements, elem_size = sizeof(char*)).
  void* arr = __tinyswift_dynarray_create_generic((int64_t)sizeof(char*));
  if (!arr) return arr;
#ifdef __APPLE__
  int argc = *_NSGetArgc();
  char** argv = *_NSGetArgv();
  for (int i = 0; i < argc; ++i) {
    char* dup = strdup(argv[i]);
    __tinyswift_dynarray_append(arr, &dup);
  }
#else
  // Linux: read /proc/self/cmdline.
  FILE* f = fopen("/proc/self/cmdline", "rb");
  if (f) {
    char buf[65536];
    size_t nread = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[nread] = '\0';
    size_t pos = 0;
    while (pos < nread) {
      char* dup = strdup(buf + pos);
      __tinyswift_dynarray_append(arr, &dup);
      pos += strlen(buf + pos) + 1;
    }
  }
#endif
  return arr;
}

char* __tinyswift_env_get(const char* key) {
  if (!key) {
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  const char* val = getenv(key);
  if (val) {
    return strdup(val);
  }
  char* empty = (char*)malloc(1);
  if (empty) empty[0] = '\0';
  return empty;
}

int64_t __tinyswift_env_set(const char* key, const char* val) {
  if (!key || !val) return 0;
  return setenv(key, val, 1) == 0 ? 1 : 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// OS: FileSystem extensions (M93)
// ═══════════════════════════════════════════════════════════════════════════════

int64_t __tinyswift_fs_mkdir(const char* path) {
  if (!path) return 0;
  return mkdir(path, 0755) == 0 ? 1 : 0;
}

void* __tinyswift_fs_listdir(const char* path) {
  void* arr = __tinyswift_dynarray_create_generic((int64_t)sizeof(char*));
  if (!arr) return arr;
  if (!path) return arr;
  DIR* dir = opendir(path);
  if (!dir) return arr;
  struct dirent* entry;
  while ((entry = readdir(dir)) != NULL) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }
    char* dup = strdup(entry->d_name);
    __tinyswift_dynarray_append(arr, &dup);
  }
  closedir(dir);
  return arr;
}

int64_t __tinyswift_fs_is_dir(const char* path) {
  if (!path) return 0;
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  return S_ISDIR(st.st_mode) ? 1 : 0;
}

int64_t __tinyswift_fs_copy(const char* src, const char* dst) {
  if (!src || !dst) return 0;
  FILE* fin = fopen(src, "rb");
  if (!fin) return 0;
  FILE* fout = fopen(dst, "wb");
  if (!fout) {
    fclose(fin);
    return 0;
  }
  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
    if (fwrite(buf, 1, n, fout) != n) {
      fclose(fin);
      fclose(fout);
      return 0;
    }
  }
  fclose(fin);
  fclose(fout);
  return 1;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Networking: TCP Sockets (M94)
// ═══════════════════════════════════════════════════════════════════════════════

int64_t __tinyswift_tcp_connect(const char* host, int64_t port) {
  if (!host) return -1;
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%lld", (long long)port);
  if (getaddrinfo(host, port_str, &hints, &res) != 0) return -1;
  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd < 0) {
    freeaddrinfo(res);
    return -1;
  }
  if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
    close(fd);
    freeaddrinfo(res);
    return -1;
  }
  freeaddrinfo(res);
  return (int64_t)fd;
}

int64_t __tinyswift_tcp_listen(int64_t port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons((uint16_t)port);
  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  if (listen(fd, 128) != 0) {
    close(fd);
    return -1;
  }
  return (int64_t)fd;
}

int64_t __tinyswift_tcp_accept(int64_t fd) {
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);
  int client_fd = accept((int)fd, (struct sockaddr*)&client_addr, &addrlen);
  return (int64_t)client_fd;
}

char* __tinyswift_tcp_read(int64_t fd, int64_t maxlen) {
  if (maxlen <= 0) maxlen = 4096;
  char* buf = (char*)malloc((size_t)maxlen + 1);
  if (!buf) {
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  ssize_t n = recv((int)fd, buf, (size_t)maxlen, 0);
  if (n <= 0) {
    free(buf);
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  buf[n] = '\0';
  return buf;
}

int64_t __tinyswift_tcp_write(int64_t fd, const char* data) {
  if (!data) return -1;
  size_t len = strlen(data);
#ifdef __APPLE__
  // macOS: SO_NOSIGPIPE is set per-socket; use send without special flags.
  ssize_t n = send((int)fd, data, len, 0);
#else
  ssize_t n = send((int)fd, data, len, MSG_NOSIGNAL);
#endif
  return (int64_t)n;
}

int64_t __tinyswift_tcp_close(int64_t fd) {
  return close((int)fd) == 0 ? 1 : 0;
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

int64_t __tinyswift_string_index_of(const char* s, const char* sub) {
  if (!s || !sub) return -1;
  const char* p = strstr(s, sub);
  if (!p) return -1;
  return (int64_t)(p - s);
}

char* __tinyswift_string_replace(const char* s, const char* target,
                                  const char* replacement) {
  if (!s) return NULL;
  if (!target || !replacement || target[0] == '\0') return strdup(s);
  size_t slen = strlen(s);
  size_t tlen = strlen(target);
  size_t rlen = strlen(replacement);
  // Count occurrences.
  size_t count = 0;
  const char* p = s;
  while ((p = strstr(p, target)) != NULL) {
    count++;
    p += tlen;
  }
  if (count == 0) return strdup(s);
  size_t result_len = slen + count * (rlen - tlen);
  char* result = (char*)malloc(result_len + 1);
  if (!result) return NULL;
  char* dst = result;
  p = s;
  while (*p) {
    if (strncmp(p, target, tlen) == 0) {
      memcpy(dst, replacement, rlen);
      dst += rlen;
      p += tlen;
    } else {
      *dst++ = *p++;
    }
  }
  *dst = '\0';
  return result;
}

char* __tinyswift_string_substring(const char* s, int64_t from, int64_t to) {
  if (!s) return NULL;
  int64_t slen = (int64_t)strlen(s);
  if (from < 0) from = 0;
  if (to > slen) to = slen;
  if (from >= to) {
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  size_t len = (size_t)(to - from);
  char* result = (char*)malloc(len + 1);
  if (!result) return NULL;
  memcpy(result, s + from, len);
  result[len] = '\0';
  return result;
}

void* __tinyswift_string_split(const char* s, const char* sep,
                                int64_t* out_count) {
  void* arr = __tinyswift_dynarray_create_generic((int64_t)sizeof(char*));
  if (out_count) *out_count = 0;
  if (!s || !sep || sep[0] == '\0') {
    if (s) {
      char* dup = strdup(s);
      __tinyswift_dynarray_append(arr, &dup);
      if (out_count) *out_count = 1;
    }
    return arr;
  }
  size_t seplen = strlen(sep);
  const char* p = s;
  int64_t count = 0;
  while (*p) {
    const char* found = strstr(p, sep);
    if (!found) {
      char* part = strdup(p);
      __tinyswift_dynarray_append(arr, &part);
      count++;
      break;
    }
    size_t partlen = (size_t)(found - p);
    char* part = (char*)malloc(partlen + 1);
    if (part) {
      memcpy(part, p, partlen);
      part[partlen] = '\0';
    }
    __tinyswift_dynarray_append(arr, &part);
    count++;
    p = found + seplen;
    if (*p == '\0') {
      char* empty = (char*)malloc(1);
      if (empty) empty[0] = '\0';
      __tinyswift_dynarray_append(arr, &empty);
      count++;
    }
  }
  if (out_count) *out_count = count;
  return arr;
}

char* __tinyswift_double_to_string(double d) {
  char buf[64];
  int len = snprintf(buf, sizeof(buf), "%.17g", d);
  char* result = (char*)malloc((size_t)len + 1);
  if (!result) return NULL;
  memcpy(result, buf, (size_t)len + 1);
  return result;
}

int64_t __tinyswift_string_to_int(const char* s, int64_t* out_success) {
  if (!s || !*s) {
    if (out_success) *out_success = 0;
    return 0;
  }
  char* end;
  long long val = strtoll(s, &end, 10);
  if (end == s || *end != '\0') {
    if (out_success) *out_success = 0;
    return 0;
  }
  if (out_success) *out_success = 1;
  return (int64_t)val;
}

double __tinyswift_string_to_double(const char* s, int64_t* out_success) {
  if (!s || !*s) {
    if (out_success) *out_success = 0;
    return 0.0;
  }
  char* end;
  double val = strtod(s, &end);
  if (end == s || *end != '\0') {
    if (out_success) *out_success = 0;
    return 0.0;
  }
  if (out_success) *out_success = 1;
  return val;
}

char* __tinyswift_string_repeated(const char* s, int64_t count) {
  if (!s || count <= 0) {
    char* empty = (char*)malloc(1);
    if (empty) empty[0] = '\0';
    return empty;
  }
  size_t slen = strlen(s);
  size_t total = slen * (size_t)count;
  char* result = (char*)malloc(total + 1);
  if (!result) return NULL;
  for (int64_t i = 0; i < count; ++i) {
    memcpy(result + i * slen, s, slen);
  }
  result[total] = '\0';
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// I/O extensions
// ═══════════════════════════════════════════════════════════════════════════════

void __tinyswift_print_double(double d) {
  printf("%.17g\n", d);
}

void __tinyswift_print_bool(int64_t b) {
  printf("%s\n", b ? "true" : "false");
}

void __tinyswift_print_newline(void) {
  putchar('\n');
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
// Dynamic array extensions
// ═══════════════════════════════════════════════════════════════════════════════

void __tinyswift_dynarray_insert(void* handle, int64_t index, const void* elem) {
  if (!handle || !elem) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (index < 0 || index > arr->count) return;
  // Ensure capacity.
  if (arr->count >= arr->capacity) {
    int64_t new_cap = arr->capacity == 0 ? 8 : arr->capacity * 2;
    void* new_data = realloc(arr->data, (size_t)new_cap * (size_t)arr->elem_size);
    if (!new_data) return;
    arr->data = new_data;
    arr->capacity = new_cap;
  }
  // Shift elements right.
  if (index < arr->count) {
    memmove((char*)arr->data + (index + 1) * arr->elem_size,
            (char*)arr->data + index * arr->elem_size,
            (size_t)(arr->count - index) * (size_t)arr->elem_size);
  }
  memcpy((char*)arr->data + index * arr->elem_size, elem,
         (size_t)arr->elem_size);
  arr->count++;
}

void __tinyswift_dynarray_remove_at(void* handle, int64_t index) {
  if (!handle) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (index < 0 || index >= arr->count) return;
  if (index < arr->count - 1) {
    memmove((char*)arr->data + index * arr->elem_size,
            (char*)arr->data + (index + 1) * arr->elem_size,
            (size_t)(arr->count - index - 1) * (size_t)arr->elem_size);
  }
  arr->count--;
}

void __tinyswift_dynarray_clear(void* handle) {
  if (!handle) return;
  ((TinySwiftDynArray*)handle)->count = 0;
}

int64_t __tinyswift_dynarray_capacity(void* handle) {
  if (!handle) return 0;
  return ((TinySwiftDynArray*)handle)->capacity;
}

void __tinyswift_dynarray_sort(void* handle,
                                int (*compare_fn)(const void*, const void*)) {
  if (!handle || !compare_fn) return;
  TinySwiftDynArray* arr = (TinySwiftDynArray*)handle;
  if (arr->count <= 1) return;
  qsort(arr->data, (size_t)arr->count, (size_t)arr->elem_size, compare_fn);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Math (libm wrappers)
// ═══════════════════════════════════════════════════════════════════════════════

double __tinyswift_sqrt(double x) { return sqrt(x); }
double __tinyswift_pow(double base, double exp) { return pow(base, exp); }
double __tinyswift_log(double x) { return log(x); }
double __tinyswift_log2(double x) { return log2(x); }
double __tinyswift_log10(double x) { return log10(x); }
double __tinyswift_floor(double x) { return floor(x); }
double __tinyswift_ceil(double x) { return ceil(x); }
double __tinyswift_round(double x) { return round(x); }
double __tinyswift_sin(double x) { return sin(x); }
double __tinyswift_cos(double x) { return cos(x); }
double __tinyswift_tan(double x) { return tan(x); }
double __tinyswift_atan2(double y, double x) { return atan2(y, x); }
double __tinyswift_fabs(double x) { return fabs(x); }
double __tinyswift_fmod(double x, double y) { return fmod(x, y); }

// ═══════════════════════════════════════════════════════════════════════════════
// Time
// ═══════════════════════════════════════════════════════════════════════════════

int64_t __tinyswift_clock_now(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

int64_t __tinyswift_clock_monotonic(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + (int64_t)ts.tv_nsec;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Filesystem/Process additions
// ═══════════════════════════════════════════════════════════════════════════════

int64_t __tinyswift_fs_is_file(const char* path) {
  if (!path) return 0;
  struct stat st;
  if (stat(path, &st) != 0) return 0;
  return S_ISREG(st.st_mode) ? 1 : 0;
}

int64_t __tinyswift_fs_rename(const char* from, const char* to) {
  if (!from || !to) return 0;
  return rename(from, to) == 0 ? 1 : 0;
}

int64_t __tinyswift_getpid(void) {
  return (int64_t)getpid();
}

void __tinyswift_env_unset(const char* key) {
  if (key) unsetenv(key);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Memory utilities
// ═══════════════════════════════════════════════════════════════════════════════

void* __tinyswift_raw_alloc(int64_t size, int64_t alignment) {
  if (size <= 0) return NULL;
  if (alignment <= 0) alignment = 8;
  void* ptr = NULL;
  posix_memalign(&ptr, (size_t)alignment, (size_t)size);
  return ptr;
}

void __tinyswift_raw_dealloc(void* ptr) {
  free(ptr);
}

void __tinyswift_memcpy(void* dst, const void* src, int64_t size) {
  if (dst && src && size > 0) memcpy(dst, src, (size_t)size);
}

void __tinyswift_memset(void* dst, int64_t value, int64_t size) {
  if (dst && size > 0) memset(dst, (int)value, (size_t)size);
}

int64_t __tinyswift_memcmp(const void* a, const void* b, int64_t size) {
  if (!a || !b || size <= 0) return 0;
  return (int64_t)memcmp(a, b, (size_t)size);
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

// ═══════════════════════════════════════════════════════════════════════════════
// Async Runtime (M100-M102)
// ═══════════════════════════════════════════════════════════════════════════════

// M100: blockOn — synchronously run an async function.
// In M100, async functions execute synchronously. blockOn simply calls the
// resume function in a loop until it completes (returns a non-suspended result).
int64_t __tinyswift_block_on(void* frame, void* (*resume_fn)(void*)) {
  if (!frame || !resume_fn) return 0;
  // For synchronous async (M100), call resume once — it runs to completion.
  return (int64_t)(intptr_t)resume_fn(frame);
}

// ── Event Loop (M101-M102) ──────────────────────────────────────────────────

typedef struct {
  void* frame;
  void* (*resume_fn)(void*);
} AsyncTask;

#define MAX_EVENT_LOOP_TASKS 256
static AsyncTask event_loop_tasks[MAX_EVENT_LOOP_TASKS];
static int event_loop_task_count = 0;

void __tinyswift_async_submit(void* frame, void* (*resume_fn)(void*)) {
  if (event_loop_task_count < MAX_EVENT_LOOP_TASKS) {
    event_loop_tasks[event_loop_task_count].frame = frame;
    event_loop_tasks[event_loop_task_count].resume_fn = resume_fn;
    event_loop_task_count++;
  }
}

void __tinyswift_run_event_loop(void) {
  // Simple round-robin: run all tasks until none remain.
  while (event_loop_task_count > 0) {
    for (int i = 0; i < event_loop_task_count; ++i) {
      AsyncTask* task = &event_loop_tasks[i];
      if (task->resume_fn) {
        void* result = task->resume_fn(task->frame);
        if (result != NULL) {
          // Task completed — remove it.
          event_loop_tasks[i] = event_loop_tasks[event_loop_task_count - 1];
          event_loop_task_count--;
          i--;  // Re-check this slot.
        }
      }
    }
  }
}

// ── I/O Polling (M102) ─────────────────────────────────────────────────────

#ifdef __APPLE__
#include <sys/event.h>
#include <unistd.h>

static int kq_fd = -1;

static void ensure_kqueue(void) {
  if (kq_fd < 0) {
    kq_fd = kqueue();
  }
}

void __tinyswift_io_poll(int64_t timeout_ms) {
  ensure_kqueue();
  struct timespec ts;
  ts.tv_sec = timeout_ms / 1000;
  ts.tv_nsec = (timeout_ms % 1000) * 1000000;
  struct kevent events[16];
  int n = kevent(kq_fd, NULL, 0, events, 16, &ts);
  for (int i = 0; i < n; ++i) {
    AsyncTask* task = (AsyncTask*)events[i].udata;
    if (task && task->resume_fn) {
      task->resume_fn(task->frame);
    }
  }
}

void __tinyswift_io_register_read(int64_t fd, void* frame,
                                   void* (*resume_fn)(void*)) {
  ensure_kqueue();
  AsyncTask* task = (AsyncTask*)malloc(sizeof(AsyncTask));
  task->frame = frame;
  task->resume_fn = resume_fn;
  struct kevent ev;
  EV_SET(&ev, (uintptr_t)fd, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, task);
  kevent(kq_fd, &ev, 1, NULL, 0, NULL);
}

void __tinyswift_io_register_write(int64_t fd, void* frame,
                                    void* (*resume_fn)(void*)) {
  ensure_kqueue();
  AsyncTask* task = (AsyncTask*)malloc(sizeof(AsyncTask));
  task->frame = frame;
  task->resume_fn = resume_fn;
  struct kevent ev;
  EV_SET(&ev, (uintptr_t)fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, task);
  kevent(kq_fd, &ev, 1, NULL, 0, NULL);
}

#else
// Linux: epoll-based I/O polling.
#include <sys/epoll.h>
#include <unistd.h>

static int epoll_fd = -1;

static void ensure_epoll(void) {
  if (epoll_fd < 0) {
    epoll_fd = epoll_create1(0);
  }
}

void __tinyswift_io_poll(int64_t timeout_ms) {
  ensure_epoll();
  struct epoll_event events[16];
  int n = epoll_wait(epoll_fd, events, 16, (int)timeout_ms);
  for (int i = 0; i < n; ++i) {
    AsyncTask* task = (AsyncTask*)events[i].data.ptr;
    if (task && task->resume_fn) {
      task->resume_fn(task->frame);
    }
  }
}

void __tinyswift_io_register_read(int64_t fd, void* frame,
                                   void* (*resume_fn)(void*)) {
  ensure_epoll();
  AsyncTask* task = (AsyncTask*)malloc(sizeof(AsyncTask));
  task->frame = frame;
  task->resume_fn = resume_fn;
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLONESHOT;
  ev.data.ptr = task;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, (int)fd, &ev);
}

void __tinyswift_io_register_write(int64_t fd, void* frame,
                                    void* (*resume_fn)(void*)) {
  ensure_epoll();
  AsyncTask* task = (AsyncTask*)malloc(sizeof(AsyncTask));
  task->frame = frame;
  task->resume_fn = resume_fn;
  struct epoll_event ev;
  ev.events = EPOLLOUT | EPOLLONESHOT;
  ev.data.ptr = task;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, (int)fd, &ev);
}

#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Test Runner Helper (M117)
// ═══════════════════════════════════════════════════════════════════════════════

#include <setjmp.h>
#include <signal.h>

static jmp_buf __tinyswift_test_jmp;
static volatile sig_atomic_t __tinyswift_test_in_test = 0;

static void __tinyswift_test_signal_handler(int sig) {
  (void)sig;
  if (__tinyswift_test_in_test) {
    longjmp(__tinyswift_test_jmp, 1);
  }
}

// Runs a test function, catching assertion failures and crashes.
// Returns 1 on success, 0 on failure.
int __tinyswift_run_test(void (*test_fn)(void)) {
  // Install signal handlers for common crash signals.
  struct sigaction sa, old_abrt, old_segv;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = __tinyswift_test_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGABRT, &sa, &old_abrt);
  sigaction(SIGSEGV, &sa, &old_segv);

  int result = 0;
  __tinyswift_test_in_test = 1;

  if (setjmp(__tinyswift_test_jmp) == 0) {
    test_fn();
    result = 1;  // Success — no crash or assertion failure.
  } else {
    result = 0;  // Failed — caught a signal.
  }

  __tinyswift_test_in_test = 0;

  // Restore original signal handlers.
  sigaction(SIGABRT, &old_abrt, NULL);
  sigaction(SIGSEGV, &old_segv, NULL);

  return result;
}

// ── Timer (M102) ────────────────────────────────────────────────────────────

void __tinyswift_timer_create(int64_t ms, void* frame,
                               void* (*resume_fn)(void*)) {
  // Simple implementation: sleep in a new thread-like way.
  // For v1 (single-threaded), just do a blocking usleep then resume.
  usleep((useconds_t)(ms * 1000));
  if (resume_fn) {
    resume_fn(frame);
  }
}
