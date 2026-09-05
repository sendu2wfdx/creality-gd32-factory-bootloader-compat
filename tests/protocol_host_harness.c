#define _GNU_SOURCE
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "board.h"
#include "platform.h"
#include "protocol.h"

enum { TEST_APP_MAP_SIZE = 256 * 1024, RX_SIZE = 4096, TX_SIZE = 4096 };
static jmp_buf escape_point;
static uint8_t rx_data[RX_SIZE], tx_data[TX_SIZE];
static size_t rx_length, rx_position, tx_length;
static unsigned flash_calls, flash_failures;
static uint32_t last_timeout;
static int jump_called;

static void fail(const char *message)
{
    fprintf(stderr, "FAIL: %s\n", message);
    exit(1);
}

static void expect(int condition, const char *message)
{
    if (!condition)
        fail(message);
}

static void reset_io(void)
{
    rx_length = rx_position = tx_length = 0;
    flash_calls = flash_failures = 0;
    last_timeout = 0;
    jump_called = 0;
}

static void append_rx(const void *data, size_t length)
{
    expect(rx_length + length <= sizeof(rx_data), "RX script overflow");
    memcpy(rx_data + rx_length, data, length);
    rx_length += length;
}

static void append_packet(const void *data, size_t length)
{
    append_rx(data, length);
    uint8_t check = bl_checksum(data, length);
    append_rx(&check, 1);
}

static void run_until_input_exhausted(void)
{
    int reason = setjmp(escape_point);
    if (reason == 0)
        bl_run(1);
    expect(reason == 1, "unexpected bootloader escape reason");
}

static void prepare_app(int valid)
{
    uint8_t *app = (uint8_t *)(uintptr_t)APP_BASE;
    const uint32_t length = 0x280;
    memset(app, 0, TEST_APP_MAP_SIZE);
    memcpy(app + APP_VERSION_OFFSET, "mcu0_022_000", 12);
    app[APP_CHECK_OFFSET + 2] = (uint8_t)length;
    app[APP_CHECK_OFFSET + 3] = (uint8_t)(length >> 8);
    app[APP_CHECK_OFFSET + 4] = (uint8_t)(length >> 16);
    app[APP_CHECK_OFFSET + 5] = (uint8_t)(length >> 24);
    uint16_t crc = 0;
    for (uint32_t i = 0; i < length; ++i) {
        uint8_t value = (i >= APP_CHECK_OFFSET && i < APP_CHECK_OFFSET + 6)
                        ? 0 : app[i];
        crc = bl_crc16_update(crc, value);
    }
    if (!valid)
        crc ^= 1;
    app[APP_CHECK_OFFSET] = (uint8_t)crc;
    app[APP_CHECK_OFFSET + 1] = (uint8_t)(crc >> 8);
}

static void test_query_sector(void)
{
    static const uint8_t command[] = {3};
    const uint8_t expected[] = {SECTOR_CODE, (uint8_t)(0xffu - SECTOR_CODE)};
    reset_io();
    append_packet(command, sizeof(command));
    run_until_input_exhausted();
    expect(tx_length == sizeof(expected), "sector response length");
    expect(memcmp(tx_data, expected, sizeof(expected)) == 0,
           "sector response bytes");
    expect(last_timeout == CONFIG_PACKET_TIMEOUT_MS, "command timeout contract");
}

static void test_query_version(int valid)
{
    static const uint8_t command[] = {0};
    uint8_t expected[26];
    prepare_app(valid);
    reset_io();
    append_packet(command, sizeof(command));
    run_until_input_exhausted();
    memcpy(expected, BOARD_ID, 12);
    expected[12] = '-';
    if (valid)
        memcpy(expected + 13, "mcu0_022_000", 12);
    else
        memset(expected + 13, 0, 12);
    expected[25] = bl_checksum(expected, 25);
    expect(tx_length == sizeof(expected), "version response length");
    expect(memcmp(tx_data, expected, sizeof(expected)) == 0,
           valid ? "valid version response" : "invalid version response");
}

static void test_start_application(int valid)
{
    static const uint8_t command[] = {2};
    static const uint8_t ack[] = {0x75, 0x8a};
    static const uint8_t bad[] = {0x1f, 0xe0};
    prepare_app(valid);
    reset_io();
    append_packet(command, sizeof(command));
    int reason = setjmp(escape_point);
    if (reason == 0)
        bl_run(1);
    if (valid) {
        expect(reason == 2, "valid app did not jump");
        expect(jump_called, "jump hook not reached");
        expect(tx_length == sizeof(ack) && memcmp(tx_data, ack, sizeof(ack)) == 0,
               "valid start response");
    } else {
        expect(reason == 1, "invalid app did not remain in command loop");
        expect(!jump_called, "invalid app jumped");
        expect(tx_length == sizeof(bad) && memcmp(tx_data, bad, sizeof(bad)) == 0,
               "invalid start response");
    }
}

static void test_update(unsigned failures, int corrupt_data,
                        uint8_t expected_final_status, unsigned expected_calls)
{
    static const uint8_t command[] = {1};
    uint8_t length_raw[4] = {0x58, 0x02, 0, 0}; /* 600 bytes */
    uint8_t image[600];
    for (size_t i = 0; i < sizeof(image); ++i)
        image[i] = (uint8_t)(i * 17u + 3u);

    reset_io();
    memset((void *)(uintptr_t)APP_BASE, 0xa5, TEST_APP_MAP_SIZE);
    flash_failures = failures;
    append_packet(command, sizeof(command));
    append_packet(length_raw, sizeof(length_raw));
    append_rx(image, sizeof(image));
    uint8_t data_check = bl_checksum(image, sizeof(image));
    if (corrupt_data)
        data_check ^= 1;
    append_rx(&data_check, 1);
    run_until_input_exhausted();

    expect(tx_length == 6, "update status packet count");
    expect(tx_data[0] == 0x75 && tx_data[2] == 0x75,
           "update did not acknowledge command and length");
    expect(tx_data[4] == expected_final_status, "update final status");
    expect(flash_calls == expected_calls, "flash retry count");
    if (expected_final_status == 0x20)
        expect(memcmp((void *)(uintptr_t)APP_BASE, image, sizeof(image)) == 0,
               "successful update did not program exact payload");
}

static void test_invalid_length(void)
{
    static const uint8_t command[] = {1};
    uint32_t invalid = MAX_APP_SIZE + 1u;
    uint8_t raw[4] = {(uint8_t)invalid, (uint8_t)(invalid >> 8),
                      (uint8_t)(invalid >> 16), (uint8_t)(invalid >> 24)};
    reset_io();
    append_packet(command, sizeof(command));
    append_packet(raw, sizeof(raw));
    run_until_input_exhausted();
    expect(tx_length == 4, "invalid length response count");
    expect(tx_data[0] == 0x75 && tx_data[2] == 0x21,
           "invalid length status");
    expect(flash_calls == 0, "invalid length reached flash");
}

static void test_length_checksum_retries(int recover)
{
    static const uint8_t command[] = {1};
    static const uint8_t raw[4] = {0x58, 0x02, 0, 0};
    uint8_t image[600];
    for (size_t i = 0; i < sizeof(image); ++i)
        image[i] = (uint8_t)(i ^ 0x5a);

    reset_io();
    append_packet(command, sizeof(command));
    unsigned bad_attempts = recover ? 1u : 5u;
    for (unsigned i = 0; i < bad_attempts; ++i) {
        append_rx(raw, sizeof(raw));
        uint8_t bad_check = (uint8_t)(bl_checksum(raw, sizeof(raw)) ^ 1u);
        append_rx(&bad_check, 1);
    }
    if (recover) {
        append_packet(raw, sizeof(raw));
        append_packet(image, sizeof(image));
    }
    run_until_input_exhausted();

    expect(tx_data[0] == 0x75, "retry test missing update ACK");
    for (unsigned i = 0; i < bad_attempts; ++i)
        expect(tx_data[2 + i * 2] == 0x1f, "bad length checksum status");
    if (recover) {
        expect(tx_length == 8, "recovered length retry response count");
        expect(tx_data[4] == 0x75 && tx_data[6] == 0x20,
               "length retry did not recover into update");
        expect(flash_calls == 1, "recovered length retry flash count");
    } else {
        expect(tx_length == 12, "five length retries response count");
        expect(flash_calls == 0, "exhausted length retries reached flash");
    }
}

void platform_init(void) {}
void platform_deinit(void) {}
uint32_t platform_millis(void)
{
    static uint32_t now;
    now += 100;
    return now;
}
int platform_uart_probe_byte(uint8_t *port, uint8_t *value)
{
    (void)port;
    (void)value;
    return 0;
}
int platform_uart_read(uint8_t port, uint8_t *value, uint32_t timeout_ms)
{
    expect(port == 1, "bootloader changed locked UART");
    last_timeout = timeout_ms;
    if (rx_position == rx_length)
        longjmp(escape_point, 1);
    *value = rx_data[rx_position++];
    return 1;
}
void platform_uart_write(uint8_t port, uint8_t value)
{
    expect(port == 1, "response used wrong UART");
    expect(tx_length < sizeof(tx_data), "TX capture overflow");
    tx_data[tx_length++] = value;
}
int platform_flash_write(uint32_t address, const uint8_t *data, size_t length)
{
    ++flash_calls;
    if (flash_failures) {
        --flash_failures;
        return -1;
    }
    memcpy((void *)(uintptr_t)address, data, length);
    return 0;
}
void platform_reset(void)
{
    fail("unexpected reset");
    __builtin_unreachable();
}
void platform_jump_to_app(void)
{
    jump_called = 1;
    longjmp(escape_point, 2);
}

int main(void)
{
    void *mapped = mmap((void *)(uintptr_t)APP_BASE, TEST_APP_MAP_SIZE,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (mapped != (void *)(uintptr_t)APP_BASE)
        fail("could not map simulated application flash");

    expect(bl_checksum((const uint8_t *)"123", 3) == 0x69,
           "compiled checksum mismatch");
    uint16_t crc = 0;
    for (const char *p = "123456789"; *p; ++p)
        crc = bl_crc16_update(crc, (uint8_t)*p);
    expect(crc == 0x31c3, "compiled CRC vector mismatch");

    test_query_sector();
    test_query_version(1);
    test_query_version(0);
    test_start_application(1);
    test_start_application(0);
    test_update(0, 0, 0x20, 1);
    test_update(2, 0, 0x20, 3);
    test_update(3, 0, 0x21, 3);
    test_update(0, 1, 0x1f, 0);
    test_invalid_length();
    test_length_checksum_retries(1);
    test_length_checksum_retries(0);

    puts("factory_bootloader_protocol_host=12/12");
    return 0;
}
