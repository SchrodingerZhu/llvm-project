/* startup.c - Bare-metal startup code and UART console for QEMU lm3s6965evb */
#include <stdint.h>
#include <stddef.h>

extern uint32_t __stack_top;
extern uint32_t __data_start, __data_end, __data_load;
extern uint32_t __bss_start, __bss_end;

extern int main(int argc, const char *argv[]);

/* Direct memory-mapped UART write for QEMU lm3s6965evb (0x4000c000) */
void baremetal_puts(const char *s) {
  while (*s) {
    *(volatile unsigned int *)0x4000c000 = *s++;
  }
}

void Reset_Handler(void) {
  /* Copy .data section from ROM to RAM */
  uint32_t *src = &__data_load;
  uint32_t *dst = &__data_start;
  while (dst < &__data_end) {
    *dst++ = *src++;
  }

  /* Zero out .bss section */
  dst = &__bss_start;
  while (dst < &__bss_end) {
    *dst++ = 0;
  }

  /* Zero out dynamic shadow memory section */
  extern uint32_t __shadow_mem_start, __shadow_mem_end;
  dst = &__shadow_mem_start;
  while (dst < &__shadow_mem_end) {
    *dst++ = 0;
  }

  baremetal_puts("\n===================================================\n");
  baremetal_puts("[BOOT] QEMU Bare-Metal Test Environment Initialized\n");
  baremetal_puts("===================================================\n");

  /* Execute constructors in .init_array */
  typedef void (*init_func_t)(void);
  extern init_func_t __init_array_start[], __init_array_end[];
  for (init_func_t *f = __init_array_start; f < __init_array_end; ++f) {
    if (*f) (*f)();
  }

#ifndef TEST_ID
#define TEST_ID 0
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

  const char *argv[] = { "asan-test", STR(TEST_ID), NULL };
  main(2, argv);

  baremetal_puts("[HALT] Test execution finished cleanly.\n");
  /* Exit QEMU or enter low power wait */
  while (1) { asm volatile("wfi"); }
}

void Default_Handler(void) {
  baremetal_puts("\n[FAULT] HardFault or Exception triggered!\n");
  while (1) { asm volatile("wfi"); }
}

__attribute__((section(".vectors"), used))
const void *vector_table[] = {
  (const void *)&__stack_top,
  (const void *)Reset_Handler,
  (const void *)Default_Handler, /* NMI */
  (const void *)Default_Handler, /* HardFault */
  (const void *)Default_Handler, /* MemManage */
  (const void *)Default_Handler, /* BusFault */
  (const void *)Default_Handler, /* UsageFault */
};
