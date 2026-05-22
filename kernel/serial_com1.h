#ifndef SERIAL_COM1_H
#define SERIAL_COM1_H

#include <stdint.h>

#define COM1_BASE 0x3F8

#if defined(__i386__) || defined(__x86_64__)
static inline void io_out8(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t io_in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
#else
/*
 * Non-x86 builds use inert stubs so the code can still be syntax-checked on
 * the host. Your real kernel build should target x86 and use the path above.
 */
static inline void io_out8(uint16_t port, uint8_t value) {
    (void)port;
    (void)value;
}

static inline uint8_t io_in8(uint16_t port) {
    (void)port;
    return 0x20;
}
#endif

static inline void com1_init(void) {
    io_out8(COM1_BASE + 1, 0x00);
    io_out8(COM1_BASE + 3, 0x80);
    io_out8(COM1_BASE + 0, 0x03);
    io_out8(COM1_BASE + 1, 0x00);
    io_out8(COM1_BASE + 3, 0x03);
    io_out8(COM1_BASE + 2, 0xC7);
    io_out8(COM1_BASE + 4, 0x0B);
}

static inline int com1_can_transmit(void) {
    return (io_in8(COM1_BASE + 5) & 0x20) != 0;
}

static inline void com1_write_char(char c) {
    while (!com1_can_transmit()) {
    }

    io_out8(COM1_BASE, (uint8_t)c);
}

static inline void com1_write_str(const char *text) {
    while (*text != '\0') {
        if (*text == '\n') {
            com1_write_char('\r');
        }

        com1_write_char(*text++);
    }
}

#endif
