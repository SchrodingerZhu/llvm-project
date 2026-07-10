/* main.cpp - Test harness for tracking boot and triggering test cases */
#include <stdint.h>

extern "C" void baremetal_puts(const char *s);

/* Clean integer printing helper without buffer reversal loop */
static void print_num(const char *label, int val) {
  baremetal_puts(label);
  if (val < 0) {
    baremetal_puts("-");
    val = -val;
  }
  if (val == 0) {
    baremetal_puts("0\n");
    return;
  }
  char digits[16];
  int temp = val;
  int len = 0;
  while (temp > 0 && len < 15) {
    len++;
    temp /= 10;
  }
  digits[len] = '\0';
  for (int i = len - 1; i >= 0; i--) {
    digits[i] = '0' + (val % 10);
    val /= 10;
  }
  baremetal_puts(digits);
  baremetal_puts("\n");
}

/* Sample global array in RAM (.data) */
int test_global_arr[3] = { 10, 20, 30 };

void trigger_global_overflow() {
  baremetal_puts("[TEST] Starting global buffer overflow test...\n");
  print_num("[TEST] Reading valid test_global_arr[0]: ", test_global_arr[0]);
  print_num("[TEST] Reading valid test_global_arr[2]: ", test_global_arr[2]);
  baremetal_puts("[TEST] Attempting out-of-bounds read at test_global_arr[3]...\n");
  
  volatile int bad_read = test_global_arr[3]; // Trigger
  print_num("[TEST] Result of bad read (if not caught by ASan): ", bad_read);
}

void trigger_stack_overflow() {
  baremetal_puts("[TEST] Starting stack buffer overflow test...\n");
  volatile int stack_arr[3] = { 1, 2, 3 };
  baremetal_puts("[TEST] Attempting out-of-bounds write at stack_arr[4]...\n");
  
  stack_arr[4] = 999; // Trigger
  baremetal_puts("[TEST] Stack overflow write completed (if not caught by ASan)\n");
}

extern "C" void run_demo(int argc, const char *argv[]);

extern "C" int main(int argc, const char *argv[]) {
  baremetal_puts("[MAIN] Entered main() test harness.\n");

  int test_id = 0;
  if (argc > 1 && argv[1]) {
    test_id = 0;
    const char *p = argv[1];
    while (*p >= '0' && *p <= '9') {
      test_id = test_id * 10 + (*p - '0');
      p++;
    }
  }

  if (test_id == 0) {
    trigger_global_overflow();
  } else if (test_id == 1) {
    trigger_stack_overflow();
  } else {
    run_demo(argc, argv);
  }

  baremetal_puts("[MAIN] Exiting main() normally.\n");
  return 0;
}
