// TinySwift Runtime — minimal ARC support (M78).
// Link with every binary that uses class types.
//
// Object layout:
//   [TinySwiftHeapHeader: {refcount: int32_t, flags: int32_t}]  ← 8 bytes, hidden
//   [field_0: T0]  ← returned pointer points HERE
//   [field_1: T1]
//   ...

#include <stdint.h>
#include <stdlib.h>

typedef struct {
  int32_t refcount;
  int32_t flags;
} TinySwiftHeapHeader;

// Allocate a heap object with the given payload size.
// Returns a pointer to the first field (header is at negative offset).
void* __tinyswift_alloc(int64_t payload_size) {
  void* raw = malloc(sizeof(TinySwiftHeapHeader) + (size_t)payload_size);
  if (!raw) return (void*)0;
  TinySwiftHeapHeader* header = (TinySwiftHeapHeader*)raw;
  header->refcount = 1;
  header->flags = 0;
  return (void*)((char*)raw + sizeof(TinySwiftHeapHeader));
}

// Increment the reference count (null-safe).
void __tinyswift_retain(void* obj) {
  if (!obj) return;
  TinySwiftHeapHeader* header =
      (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));
  header->refcount++;
}

// Decrement the reference count. If it reaches zero, call the deinit
// function (if non-null), then free the memory.
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

// Check if the object has exactly one reference (for future COW).
int64_t __tinyswift_is_unique(void* obj) {
  if (!obj) return 0;
  TinySwiftHeapHeader* header =
      (TinySwiftHeapHeader*)((char*)obj - sizeof(TinySwiftHeapHeader));
  return header->refcount == 1 ? 1 : 0;
}
