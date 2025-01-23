#include <stddef.h>

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int bool;
#define true 1
#define false 0

#define UART0_BASE 0x10000000
#define UART_REG(reg) ((volatile uint8_t *)(UART0_BASE + reg))
#define UART_THR 0x00
#define UART_LSR 0x05
#define BUFFER_SIZE 5

// Mutex implementation
typedef enum {
    MUTEX_UNLOCKED = 0,
    MUTEX_LOCKED = 1
} mutex_t;

bool mutex_trylock(mutex_t *mutex) {
    int old;
    asm volatile (
        "amoswap.w.aq %0, %1, (%2)"
        : "=r"(old)
        : "r"(MUTEX_LOCKED), "r"(mutex)
        : "memory"
    );
    return old != MUTEX_LOCKED;
}

void mutex_lock(mutex_t *mutex) {
    while (!mutex_trylock(mutex));
}

void mutex_unlock(mutex_t *mutex) {
    asm volatile (
        "amoswap.w.rl zero, zero, (%0)"
        :
        : "r"(mutex)
        : "memory"
    );
}

// Condition variable implementation
typedef struct {
    mutex_t lock;
    volatile uint32_t waiting;
    volatile uint32_t to_wake;
} cond_t;

void cond_init(cond_t *cv) {
    cv->waiting = 0;
    cv->to_wake = 0;
}

void cond_wait(cond_t *cv, mutex_t *mutex) {
    mutex_lock(&cv->lock);
    cv->waiting++;
    mutex_unlock(&cv->lock);
    mutex_unlock(mutex);

    while (1) {
        mutex_lock(&cv->lock);
        if (cv->to_wake > 0) {
            cv->to_wake--;
            cv->waiting--;
            mutex_unlock(&cv->lock);
            break;
        }
        mutex_unlock(&cv->lock);
        for (volatile int i = 0; i < 100; i++);
    }

    mutex_lock(mutex);
}

void cond_signal(cond_t *cv) {
    mutex_lock(&cv->lock);
    if (cv->waiting > 0) {
        cv->to_wake++;
    }
    mutex_unlock(&cv->lock);
}

// UART functions
void uart_putc(char c) {
    while ((*UART_REG(UART_LSR) & 0x20) == 0);
    *UART_REG(UART_THR) = c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

void print_uint(uint32_t val) {
    char buf[12];
    int i = 0;
    do {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    } while (val);
    while (i > 0) uart_putc(buf[--i]);
}

// Circular buffer implementation
typedef struct {
    volatile uint32_t items[BUFFER_SIZE];
    volatile uint32_t read_pos;
    volatile uint32_t write_pos;
    volatile uint32_t count;
} circular_buffer_t;

void buffer_init(circular_buffer_t *buf) {
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->count = 0;
}

bool buffer_is_full(circular_buffer_t *buf) {
    return buf->count == BUFFER_SIZE;
}

bool buffer_is_empty(circular_buffer_t *buf) {
    return buf->count == 0;
}

void buffer_put(circular_buffer_t *buf, uint32_t item) {
    buf->items[buf->write_pos] = item;
    buf->write_pos = (buf->write_pos + 1) % BUFFER_SIZE;
    buf->count++;
}

uint32_t buffer_get(circular_buffer_t *buf) {
    uint32_t item = buf->items[buf->read_pos];
    buf->read_pos = (buf->read_pos + 1) % BUFFER_SIZE;
    buf->count--;
    return item;
}

// Test variables
static mutex_t mutex = MUTEX_UNLOCKED;
static cond_t not_empty = {0};
static cond_t not_full = {0};
static circular_buffer_t buffer = {0};
static volatile bool producer_done = false;

void producer_task(void) {
    uint32_t item = 0;
    uart_puts("Producer starting\n");

    for (int i = 0; i < 10; i++) {
        mutex_lock(&mutex);

        while (buffer_is_full(&buffer)) {
            uart_puts("Buffer full, producer waiting\n");
            cond_wait(&not_full, &mutex);
        }

        item++;
        buffer_put(&buffer, item);
        uart_puts("Produced item ");
        print_uint(item);
        uart_puts("\n");

        cond_signal(&not_empty);
        mutex_unlock(&mutex);

        // Simulate work
        for (volatile int j = 0; j < 1000; j++);
    }

    producer_done = true;
}

void consumer_task(void) {
    uart_puts("Consumer starting\n");

    while (!producer_done || !buffer_is_empty(&buffer)) {
        mutex_lock(&mutex);

        while (!producer_done && buffer_is_empty(&buffer)) {
            uart_puts("Buffer empty, consumer waiting\n");
            cond_wait(&not_empty, &mutex);
        }

        if (!buffer_is_empty(&buffer)) {
            uint32_t item = buffer_get(&buffer);
            uart_puts("Consumed item ");
            print_uint(item);
            uart_puts("\n");
            cond_signal(&not_full);
        }

        mutex_unlock(&mutex);

        // Simulate work
        for (volatile int j = 0; j < 1000; j++);
    }
}

void main(void) {
    buffer_init(&buffer);
    cond_init(&not_empty);
    cond_init(&not_full);

    unsigned int hartid;
    asm volatile("csrr %0, mhartid" : "=r"(hartid));

    if (hartid == 0) {
        producer_task();
    } else if (hartid == 1) {
        consumer_task();
    }

    while(1);
}
