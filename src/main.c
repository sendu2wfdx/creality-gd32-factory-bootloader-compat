#include "platform.h"
#include "protocol.h"
#include "board.h"

int main(void)
{
    platform_init();
    uint32_t start = platform_millis();
    for (;;) {
        uint8_t port, value;
        if (platform_uart_probe_byte(&port, &value) && value == 0x75u) {
            platform_uart_write(port, 0x75u); /* initial reply is unframed */
            bl_run(port);
        }
        if ((uint32_t)(platform_millis() - start) >= CONFIG_BOOT_WAIT_MS
            && bl_application_valid())
            platform_jump_to_app();
    }
}
