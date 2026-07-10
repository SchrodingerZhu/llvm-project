#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <alloca.h>


void demo_use_after_free() {
  int *p = (int*)malloc(sizeof(int));
  printf("p: %p\n", p);
  *p = 42;
  printf("*p: %d\n", *p);
  free(p);
  printf("*p: %d\n", *p);
}

template<typename T, int N>
void demo_heap_overflow() {
  T *p = (T*)malloc(sizeof(T) * N);
  memset(p, 42, sizeof(T) * N);
  printf("p: %p\n", p);
  printf("p[%d]: %#x\n", N - 1, p[N - 1]);
  printf("p[%d]: %#x\n", N    , p[N    ]);
}

void demo_stack_overflow(int n) {
  int a[3];
  int bbb[3];
  a[0] = a[1] = a[2] = 42;
  printf("a[0]: %d\n", a[0]);
  printf("a[%d]: %d\n", n, a[n]);
  bbb[0] = bbb[1] = bbb[2] = 42;
  printf("bbb[0]: %d\n", bbb[0]);
  printf("bbb[%d]: %d\n", n, bbb[n]);
}

const int cga[3] = {0, 1, 2};
int ga[3] = {0, 1, 2};

void demo_global_overflow(int *ptr) {
  printf("ptr[0]: %d\n", ptr[0]);
  printf("ptr[1]: %d\n", ptr[1]);
  printf("ptr[2]: %d\n", ptr[2]);
  printf("ptr[3]: %d\n", ptr[3]);
}

void demo_isupper(int c) {
  printf("isupper(%c) = %d\n", (char)c, isupper(c));
}

void demo_errno() {
  printf("%s\n", strerror(errno));
}

void demo_stack_overflow_alloca_use(char *foo, int size) {
  foo[size-1] = 42;
  printf("Wrote to offset %d\n", size - 1);
  foo[size] = 42;
  printf("Wrote to offset %d\n", size);
}

void demo_stack_overflow_alloca(int size) {
  char *foo = (char*)alloca(size);
  printf("Created alloca of %d bytes\n", size);
  demo_stack_overflow_alloca_use(foo, size);
}

int *demo_use_after_return_inner() {
  int a = 42;
  int *pa = &a;
  asm volatile("" : "+r" (pa));
  return pa;
}

void demo_use_after_return() {
  int *pa = demo_use_after_return_inner();
  printf("*pa: %d\n", *pa);
}

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <n>\n", argv[0]);
    return 1;
  }
  int test_num = atoi(argv[1]);
  switch (test_num) {
  case 0:
    printf("Running demo_use_after_free:\n");
    demo_use_after_free();
    break;
  case 1:
    printf("Running demo_heap_overflow:\n");
    demo_heap_overflow<short, 3>();
    break;
  case 2:
    printf("Running demo_stack_overflow:\n");
    demo_stack_overflow(3);
    break;
  case 3:
    printf("Running demo_global_overflow:\n");
    demo_global_overflow(ga);
    break;
  case 4:
    printf("Running demo_global_overflow with const global:\n");
    demo_global_overflow((int*)cga);
    break;
  case 5:
    printf("Running demo_isupper:\n");
    demo_isupper((int)'A');
    break;
  case 6:
    printf("Running demo_errno:\n");
    demo_errno();
    break;
  case 7:
    printf("Running demo_stack_overflow_alloca:\n");
    demo_stack_overflow_alloca(19);
    break;
  case 8:
    printf("Running demo_use_after_return:\n");
    demo_use_after_return();
    break;
  default:
    printf("Invalid test num\n");
    break;
  }
}
