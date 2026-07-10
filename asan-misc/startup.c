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

  /* Copy .shadow_rw section from ROM to RAM */
  extern uint32_t __shadow_rw_load, __shadow_rw_start, __shadow_rw_end;
  src = &__shadow_rw_load;
  dst = &__shadow_rw_start;
  while (dst < &__shadow_rw_end) {
    *dst++ = *src++;
  }

#ifndef TEST_ID
#define TEST_ID 0
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

  baremetal_puts("\n===================================================\n");
  baremetal_puts("[BOOT] QEMU Bare-Metal Test Environment Initialized\n");
  baremetal_puts("===================================================\n");

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
