#include "baremetal_libc.h"
#include "src/stdlib/malloc.h"
#include "src/stdlib/free.h"

extern "C" void baremetal_puts(const char *s);
extern uint32_t __stack_top;
extern char Image$$STACKHEAP_START$$Base;
extern char Image$$STACKHEAP_SPLIT$$Base;

extern "C" {

void *memcpy(void *dest, const void *src, size_t n) {
  char *d = (char *)dest;
  const char *s = (const char *)src;
  while (n--) *d++ = *s++;
  return dest;
}

void *memset(void *s, int c, size_t n) {
  unsigned char *p = (unsigned char *)s;
  while (n--) *p++ = (unsigned char)c;
  return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  while (n--) {
    if (*p1 != *p2) return *p1 - *p2;
    p1++; p2++;
  }
  return 0;
}

size_t strlen(const char *s) {
  size_t len = 0;
  while (*s++) len++;
  return len;
}

void abort(void) {
  baremetal_puts("\n[ABORTED]\n");
  while (1) {}
}

void *malloc(size_t size) {
  return LIBC_NAMESPACE::malloc(size);
}

void free(void *ptr) {
  LIBC_NAMESPACE::free(ptr);
}

long long strtoll(const char *nptr, char **endptr, int base) {
  while (*nptr == ' ') nptr++;
  long long res = 0;
  while (*nptr >= '0' && *nptr <= '9') {
    res = res * 10 + (*nptr - '0');
    nptr++;
  }
  if (endptr) *endptr = (char *)nptr;
  return res;
}

int isupper(int c) {
  return (c >= 'A' && c <= 'Z');
}

int atoi(const char *nptr) {
  while (*nptr == ' ') nptr++;
  int sign = 1;
  if (*nptr == '-') { sign = -1; nptr++; }
  else if (*nptr == '+') { nptr++; }
  int res = 0;
  while (*nptr >= '0' && *nptr <= '9') {
    res = res * 10 + (*nptr - '0');
    nptr++;
  }
  return sign * res;
}

char *strerror(int errnum) {
  (void)errnum;
  return (char *)"baremetal error";
}

static void print_hex(unsigned long long val, int width) {
  char buf[20];
  int pos = 0;
  if (val == 0) {
    buf[pos++] = '0';
  } else {
    while (val > 0 && pos < 18) {
      int d = val & 0xf;
      buf[pos++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
      val >>= 4;
    }
  }
  while (pos < width && pos < 18) {
    buf[pos++] = '0';
  }
  for (int i = pos - 1; i >= 0; i--) {
    char c[2] = { buf[i], 0 };
    baremetal_puts(c);
  }
}

static void print_dec(long long val) {
  if (val < 0) {
    baremetal_puts("-");
    val = -val;
  }
  if (val == 0) {
    baremetal_puts("0");
    return;
  }
  char buf[20];
  int pos = 0;
  while (val > 0 && pos < 18) {
    buf[pos++] = '0' + (val % 10);
    val /= 10;
  }
  for (int i = pos - 1; i >= 0; i--) {
    char c[2] = { buf[i], 0 };
    baremetal_puts(c);
  }
}

int printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  while (*fmt) {
    if (*fmt != '%') {
      char c[2] = { *fmt++, 0 };
      baremetal_puts(c);
      continue;
    }
    fmt++; // skip '%'
    // parse optional flags / precision
    bool alt = false;
    if (*fmt == '#') { alt = true; fmt++; }
    int width = 0;
    while (*fmt == '0') fmt++;
    while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }
    if (*fmt == '.') {
      fmt++;
      while (*fmt == '*' || (*fmt >= '0' && *fmt <= '9')) fmt++;
    }
    // length modifiers
    int long_cnt = 0;
    while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
      if (*fmt == 'l') long_cnt++;
      if (*fmt == 'z') long_cnt = 1; // size_t/uintptr_t is 32-bit (unsigned long) on Cortex-M
      fmt++;
    }
    char spec = *fmt++;
    if (spec == 's') {
      const char *s = va_arg(args, const char *);
      if (!s) s = "(null)";
      baremetal_puts(s);
    } else if (spec == 'c') {
      char c[2] = { (char)va_arg(args, int), 0 };
      baremetal_puts(c);
    } else if (spec == 'd' || spec == 'i') {
      long long v = (long_cnt >= 2) ? va_arg(args, long long) : ((long_cnt == 1) ? va_arg(args, long) : va_arg(args, int));
      print_dec(v);
    } else if (spec == 'u') {
      unsigned long long v = (long_cnt >= 2) ? va_arg(args, unsigned long long) : ((long_cnt == 1) ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
      print_dec((long long)v);
    } else if (spec == 'x' || spec == 'X') {
      if (alt) baremetal_puts("0x");
      unsigned long long v = (long_cnt >= 2) ? va_arg(args, unsigned long long) : ((long_cnt == 1) ? va_arg(args, unsigned long) : va_arg(args, unsigned int));
      print_hex(v, width);
    } else if (spec == 'p') {
      baremetal_puts("0x");
      unsigned long long v = (unsigned long long)(uintptr_t)va_arg(args, void *);
      print_hex(v, 8);
    } else if (spec == '%') {
      baremetal_puts("%");
    }
  }
  va_end(args);
  return 0;
}

} // extern "C"
