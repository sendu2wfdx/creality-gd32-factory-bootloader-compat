#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_BASE 0x08003000u
#define APP_VERSION_OFFSET 0x200u
#define APP_CHECK_OFFSET 0x20cu

void platform_init(void);
void platform_deinit(void);
uint32_t platform_millis(void);
int platform_uart_probe_byte(uint8_t *port, uint8_t *value);
int platform_uart_read(uint8_t port, uint8_t *value, uint32_t timeout_ms);
void platform_uart_write(uint8_t port, uint8_t value);
int platform_flash_write(uint32_t address, const uint8_t *data, size_t length);
void platform_reset(void) __attribute__((noreturn));
void platform_jump_to_app(void) __attribute__((noreturn));
