#include "board.h"
#include "platform.h"
#include "protocol.h"

enum { ACK = 0x75, DONE = 0x20, BAD_SUM = 0x1f, FLASH_ERR = 0x21 };
static uint8_t block[BLOCK_SIZE];
static const char board_id[12]
    __attribute__((section(".board_id"), used)) = BOARD_ID;

uint8_t bl_checksum(const uint8_t *data, size_t length)
{
    uint8_t sum = 0;
    while (length--) sum = (uint8_t)(sum + *data++);
    return (uint8_t)(0xffu - sum);
}

uint16_t bl_crc16_update(uint16_t crc, uint8_t value)
{
    crc ^= (uint16_t)value << 8;
    for (unsigned i = 0; i < 8; ++i)
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                              : (uint16_t)(crc << 1);
    return crc;
}

static uint32_t read32(uintptr_t address)
{
    const volatile uint8_t *p = (const volatile uint8_t *)address;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
           | ((uint32_t)p[3] << 24);
}

bool bl_application_valid(void)
{
    uint32_t length = read32(APP_BASE + APP_CHECK_OFFSET + 2u);
    if (length < APP_CHECK_OFFSET + 6u || length > MAX_APP_SIZE)
        return false;
    uint16_t expected = *(const volatile uint16_t *)(APP_BASE + APP_CHECK_OFFSET);
    uint16_t crc = 0;
    const volatile uint8_t *image = (const volatile uint8_t *)APP_BASE;
    for (uint32_t i = 0; i < length; ++i) {
        uint8_t value = (i >= APP_CHECK_OFFSET && i < APP_CHECK_OFFSET + 6u)
                        ? 0u : image[i];
        crc = bl_crc16_update(crc, value);
    }
    return crc == expected;
}

static void send_packet(uint8_t port, const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; ++i) platform_uart_write(port, data[i]);
    platform_uart_write(port, bl_checksum(data, length));
}

static bool receive_packet(uint8_t port, uint8_t *data, size_t length)
{
    uint8_t check;
    for (size_t i = 0; i < length; ++i)
        if (!platform_uart_read(port, &data[i], CONFIG_PACKET_TIMEOUT_MS)) return false;
    return platform_uart_read(port, &check, CONFIG_PACKET_TIMEOUT_MS)
           && check == bl_checksum(data, length);
}

static void send_status(uint8_t port, uint8_t status)
{
    send_packet(port, &status, 1);
}

static void send_version(uint8_t port)
{
    uint8_t version[25];
    for (unsigned i = 0; i < 12; ++i) version[i] = (uint8_t)board_id[i];
    version[12] = '-';
    bool valid = bl_application_valid();
    for (unsigned i = 0; i < 12; ++i)
        version[13 + i] = valid
            ? *(const volatile uint8_t *)(uintptr_t)
                (APP_BASE + APP_VERSION_OFFSET + i) : 0;
    send_packet(port, version, sizeof(version));
}

static bool receive_length(uint8_t port, uint32_t *length)
{
    uint8_t raw[4];
    for (unsigned attempt = 0; attempt < 5; ++attempt) {
        if (!receive_packet(port, raw, sizeof(raw))) {
            send_status(port, BAD_SUM);
            continue;
        }
        *length = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8)
                  | ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
        if (*length >= APP_CHECK_OFFSET + 6u && *length <= MAX_APP_SIZE) {
            send_status(port, ACK);
            return true;
        }
        send_status(port, FLASH_ERR);
        return false;
    }
    return false;
}

static void update(uint8_t port)
{
    uint32_t length;
    send_status(port, ACK);
    if (!receive_length(port, &length)) return;
    uint32_t offset = 0;
    while (offset < length) {
        size_t count = length - offset;
        if (count > BLOCK_SIZE) count = BLOCK_SIZE;
        /* A bad wire checksum aborts this download.  The factory host rewinds
         * the file and begins again at update_request. */
        if (!receive_packet(port, block, count)) {
            send_status(port, BAD_SUM);
            return;
        }
        bool written = false;
        /* The original retries flash programming from the same RAM buffer;
         * it does not request the data block again between these attempts. */
        for (unsigned attempt = 0; attempt < 3 && !written; ++attempt)
            written = platform_flash_write(APP_BASE + offset, block, count) == 0;
        if (!written) {
            send_status(port, FLASH_ERR);
            return;
        }
        offset += (uint32_t)count;
        send_status(port, offset == length ? DONE : ACK);
    }
}

void bl_run(uint8_t port)
{
    for (;;) {
        uint8_t command[1];
        uint8_t check;
        if (!platform_uart_read(port, command, CONFIG_PACKET_TIMEOUT_MS)) continue;
        if (!platform_uart_read(port, &check, CONFIG_PACKET_TIMEOUT_MS)
            || check != bl_checksum(command, 1)) continue;
        switch (command[0]) {
        case 0: send_version(port); break;
        case 1: update(port); break;
        case 2:
            if (!bl_application_valid()) {
                send_status(port, BAD_SUM);
                break;
            }
            send_status(port, ACK);
            /* All three originals delay 1000 ms before clearing the MCU state
             * and branching through the application vector table. */
            {
                uint32_t start = platform_millis();
                while ((uint32_t)(platform_millis() - start) < 1000u) {}
            }
            platform_jump_to_app();
            break;
        case 3: {
            const uint8_t sector = SECTOR_CODE;
            send_packet(port, &sector, 1);
            break;
        }
        default: break;
        }
    }
}
