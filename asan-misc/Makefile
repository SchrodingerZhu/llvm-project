# Makefile for asan-misc baremetal QEMU environment
CC = clang
CXX = clang
LD = ld.lld
CFLAGS = --target=arm-none-eabi -mcpu=cortex-m3 -mthumb -O1 -g -ffreestanding -Wno-array-bounds
LDFLAGS = -T baremetal.ld --fatal-warnings

# Once clang is rebuilt with new options, you can add:
# ASAN_FLAGS = -fsanitize=address -mllvm -asan-split-shadow -mllvm -asan-create-global-shadow

SRCS = startup.c main.cpp asan-runtime.cpp asan-demo.cpp
OBJS = startup.o main.o asan-runtime.o asan-demo.o
TARGET = firmware.elf

all: $(TARGET)

startup.o: startup.c
	$(CC) $(CFLAGS) -c $< -o $@

main.o: main.cpp
	$(CXX) $(CFLAGS) $(ASAN_FLAGS) -c $< -o $@

asan-runtime.o: asan-runtime.cpp
	$(CXX) $(CFLAGS) -DSPLIT_SHADOW -DSMALL_GLOBAL_METADATA -c $< -o $@

asan-demo.o: asan-demo.cpp
	$(CXX) $(CFLAGS) $(ASAN_FLAGS) -DSPLIT_SHADOW -DSMALL_GLOBAL_METADATA -c $< -o $@

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

run: $(TARGET)
	@echo "Running in QEMU (Ctrl+A then X to exit if stuck)..."
	qemu-system-arm -M lm3s6965evb -nographic -kernel $(TARGET) & \
	QEMU_PID=$$!; sleep 1; kill $$QEMU_PID 2>/dev/null || true

clean:
	rm -f $(OBJS) $(TARGET) test_uart.*
