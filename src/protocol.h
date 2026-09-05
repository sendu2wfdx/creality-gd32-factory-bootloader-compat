#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint8_t bl_checksum(const uint8_t *data, size_t length);
uint16_t bl_crc16_update(uint16_t crc, uint8_t value);
bool bl_application_valid(void);
void bl_run(uint8_t port) __attribute__((noreturn));
