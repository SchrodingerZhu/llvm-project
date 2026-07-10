#ifndef BAREMETAL_LIBC_H
#define BAREMETAL_LIBC_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Memory functions
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);

// Allocator
void *malloc(size_t size);
void free(void *ptr);

// Abort and console
void abort(void);
int printf(const char *fmt, ...);

// Conversion
long long strtoll(const char *nptr, char **endptr, int base);
int atoi(const char *nptr);
int isupper(int c);
char *strerror(int errnum);

// Assert and errno
#define assert(x) do { if (!(x)) { printf("ASSERT FAILED: %s at %s:%d\n", #x, __FILE__, __LINE__); abort(); } } while (0)
#define errno 0

// PRIxPTR
#define PRIxPTR "lx"

#ifndef alloca
#define alloca(size) __builtin_alloca(size)
#endif

#ifdef __cplusplus
} // extern "C"

// C++ std helpers
namespace std {
  template<typename T>
  inline const T& max(const T& a, const T& b) {
    return (a < b) ? b : a;
  }
  template<typename T>
  inline const T& min(const T& a, const T& b) {
    return (b < a) ? b : a;
  }
  template<typename T>
  inline const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : ((hi < v) ? hi : v);
  }
}
#endif

#endif // BAREMETAL_LIBC_H
