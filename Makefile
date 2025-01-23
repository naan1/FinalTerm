CC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
CFLAGS = -march=rv32ima -mabi=ilp32 -nostdlib -mcmodel=medany
LDFLAGS = -T link.ld

TARGET = mutex_test
SRCS = mutex_test.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	qemu-system-riscv32 -machine virt -cpu rv32 -smp 2 -kernel $(TARGET) -nographic
