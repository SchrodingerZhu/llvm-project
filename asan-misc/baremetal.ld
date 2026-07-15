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
    Image$$ER_RO_DATA$$Base = .;
    *(.rodata*)
    . = ALIGN(4);
    Image$$ER_RO_DATA$$Limit = .;
  } > ROM

  .init_array : ALIGN(4) {
    __init_array_start = .;
    KEEP(*(.init_array*))
    KEEP(*(.ctors*))
    __init_array_end = .;
  } > ROM

  .ARM.extab : ALIGN(4) { *(.ARM.extab* .gnu.linkonce.armextab.*) } > ROM
  .ARM.exidx : ALIGN(4) { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > ROM

  /* ASan Global Metadata Section */
  .asan_metadata : ALIGN(8) {
    __asan_global_metadata$$Base = .;
    *(__asan_global_metadata*)
    __asan_global_metadata$$Limit = .;
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

  /* Reserve ASan Dynamic Shadow Memory Region in RAM at 0x20004000 */
  .shadow_mem 0x20004000 (NOLOAD) : ALIGN(8) {
    __shadow_mem_start = .;
    . = . + 8K; /* 8KB Dynamic Shadow Space */
    __shadow_mem_end = .;
  } > RAM

  .stack_heap : ALIGN(8) {
    Image$$STACKHEAP_START$$Base = .;
    _end = .;
    . = . + 12K; /* Heap */
    __llvm_libc_heap_limit = .;
    Image$$STACKHEAP_SPLIT$$Base = .;
    . = . + 12K; /* Stack */
    Image$$STACKHEAP_END$$Limit = .;
    __stack_top = .;
  } > RAM
}
