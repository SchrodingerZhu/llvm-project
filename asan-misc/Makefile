# Makefile for asan-misc baremetal QEMU environment
CC = ../build/bin/clang
CXX = ../build/bin/clang
LD = ../build/bin/ld.lld
CFLAGS = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -O1 -g -ffreestanding -Wno-array-bounds -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer
LDFLAGS = -T baremetal.ld --fatal-warnings --asan-shadow-mode=split --asan-shadow-offset=32768 --wrap=malloc --wrap=free

# Once clang is rebuilt with new options, you can add:
ASAN_FLAGS = -fsanitize=address -mllvm -asan-split-shadow -mllvm -asan-mapping-offset=32768 -mllvm -asan-create-global-shadow -mllvm -asan-globals-metadata-section -mllvm -asan-small-global-metadata

SRCS = startup.c main.cpp baremetal_libc.cpp asan-runtime.cpp asan-demo.cpp
OBJS = startup.o main.o baremetal_libc.o asan-runtime.o asan-demo.o
TARGET = firmware.elf

all: $(TARGET)

TEST ?= 0

startup.o: startup.c
	$(CC) $(CFLAGS) -DTEST_ID=$(TEST) -c $< -o $@

main.o: main.cpp
	$(CXX) $(CFLAGS) $(ASAN_FLAGS) -O0 -DTEST_ID=$(TEST) -c $< -o $@

baremetal_libc.o: baremetal_libc.cpp
	$(CXX) $(CFLAGS) -c $< -o $@

asan-runtime.o: asan-runtime.cpp
	$(CXX) $(CFLAGS) -DSPLIT_SHADOW -DSMALL_GLOBAL_METADATA -c $< -o $@

asan-demo.o: asan-demo.cpp
	$(CXX) $(CFLAGS) $(ASAN_FLAGS) -O0 -DSPLIT_SHADOW -DSMALL_GLOBAL_METADATA -c $< -o $@

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

run: $(TARGET)
	@echo "Running in QEMU (Ctrl+A then X to exit if stuck)..."
	qemu-system-arm -M lm3s6965evb -nographic -kernel $(TARGET) > qemu_run.log 2>&1 & \
	QEMU_PID=$$!; sleep 1; kill $$QEMU_PID 2>/dev/null || true
	@cat qemu_run.log | python3 asan_symbolize.py --elf $(TARGET)

run-all:
	@for i in 0 1 2 3 4 5 6 7 8; do \
		echo "\n=========================================================="; \
		echo "=== Running ASan Demo Case $$i ==="; \
		echo "=========================================================="; \
		$(MAKE) -s clean; \
		$(MAKE) -s $(TARGET) TEST=$$i; \
		qemu-system-arm -M lm3s6965evb -nographic -kernel $(TARGET) > qemu_test_$$i.log 2>&1 & \
		QEMU_PID=$$!; sleep 1; kill $$QEMU_PID 2>/dev/null || true; \
		cat qemu_test_$$i.log | python3 asan_symbolize.py --elf $(TARGET); \
	done

clean:
	rm -f $(OBJS) $(TARGET) test_uart.*
