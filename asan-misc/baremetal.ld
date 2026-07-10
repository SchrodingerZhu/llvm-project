/* baremetal.ld - Linker script for QEMU Cortex-M lm3s6965evb (64KB SRAM, 256KB Flash) */
ENTRY(Reset_Handler)

MEMORY {
  ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS {
  .text : ALIGN(4) {
    __vectors_start = .;
    KEEP(*(.vectors))
    *(.text*)
    . = ALIGN(4);
  } > ROM

  .rodata : ALIGN(4) {
    *(.rodata*)
    . = ALIGN(4);
  } > ROM

  /* ASan Global Metadata Section */
  .asan_metadata : ALIGN(8) {
    __asan_global_metadata$$Base = .;
    *(__asan_global_metadata*)
    __asan_global_metadata$$Limit = .;
  } > ROM

  /* ASan Static ROM Shadow Section */
  .shadow_ro : ALIGN(8) {
    Image$$ER_SHADOW_ROM$$Base = .;
    *(__shadow_ro*)
    Image$$ER_RO_DATA$$Base = .; /* Runtime symbol for ROM end */
    Image$$ER_SHADOW_ROM$$Limit = .;
  } > ROM

  .data : ALIGN(4) {
    __data_start = .;
    Image$$ER_RW_ZI$$Base = .;
    *(.data*)
    . = ALIGN(4);
    __data_end = .;
  } > RAM AT > ROM

  __data_load = LOADADDR(.data);

  .bss : ALIGN(4) {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
    Image$$ER_RW_ZI$$Limit = .;
  } > RAM

  /* ASan Static RAM Shadow Section */
  .shadow_rw : ALIGN(8) {
    Image$$ER_SHADOW_1$$Base = .;
    *(__shadow_rw*)
    Image$$ER_SHADOW_4$$Limit = .;
  } > RAM

  /* Stack and Heap boundaries within 64KB SRAM */
  .stack_heap : ALIGN(8) {
    Image$$STACKHEAP_START$$Base = .;
    . = . + 16K; /* Heap */
    Image$$STACKHEAP_SPLIT$$Base = .;
    . = . + 16K; /* Stack */
    Image$$STACKHEAP_END$$Limit = .;
    __stack_top = .;
  } > RAM
}
