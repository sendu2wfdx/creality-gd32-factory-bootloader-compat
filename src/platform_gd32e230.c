#include "platform.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define SYST_CSR REG32(0xe000e010u)
#define SYST_RVR REG32(0xe000e014u)
#define SYST_CVR REG32(0xe000e018u)
#define SCB_VTOR REG32(0xe000ed08u)
#define SCB_AIRCR REG32(0xe000ed0cu)
#define RCU_CTL0 REG32(0x40021000u)
#define RCU_CFG0 REG32(0x40021004u)
#define RCU_AHBEN REG32(0x40021014u)
#define RCU_APB2EN REG32(0x40021018u)
#define RCU_APB1EN REG32(0x4002101cu)
#define GPIOA_MODER REG32(0x48000000u)
#define GPIOA_AFRL REG32(0x48000020u)
#define GPIOA_AFRH REG32(0x48000024u)
#define USART0_BASE 0x40013800u
#define USART1_BASE 0x40004400u
#define USART_STAT(b) REG32((b) + 0x1cu)
#define USART_RDATA(b) REG32((b) + 0x24u)
#define USART_TDATA(b) REG32((b) + 0x28u)
#define USART_BAUD(b) REG32((b) + 0x0cu)
#define USART_CTL0(b) REG32((b) + 0x00u)

static uint32_t ticks;
static const uint32_t ports[2] = { USART0_BASE, USART1_BASE };
void SysTick_Handler(void) { ++ticks; }

static int clock_setup(void)
{
    uint32_t timeout = 0xfffffu;
    RCU_CTL0 |= 1u << 16;
    while (!(RCU_CTL0 & (1u << 17)) && --timeout) {}
    if (!timeout) return -1;
    REG32(0x40022000u) = (REG32(0x40022000u) & ~7u) | 2u;
    RCU_CFG0 &= ~((15u << 4) | (7u << 8) | (7u << 11)
                  | (1u << 16) | (15u << 18) | (1u << 27)
                  | (1u << 31));
    RCU_CFG0 |= (1u << 16) | (7u << 18); /* HXTAL x9 = 72 MHz */
    RCU_CTL0 |= 1u << 24;
    timeout = 0xfffffu;
    while (!(RCU_CTL0 & (1u << 25)) && --timeout) {}
    if (!timeout) return -1;
    RCU_CFG0 = (RCU_CFG0 & ~3u) | 2u;
    timeout = 0xfffffu;
    while ((RCU_CFG0 & 12u) != 8u && --timeout) {}
    return timeout ? 0 : -1;
}

void platform_init(void)
{
    SCB_VTOR = 0x08000000u;
    if (clock_setup()) platform_reset();
    RCU_AHBEN |= (1u << 17);
    RCU_APB2EN |= (1u << 14);
    RCU_APB1EN |= (1u << 17);
    GPIOA_MODER = (GPIOA_MODER & ~((15u << 4) | (15u << 18)))
                  | (10u << 4) | (10u << 18);
    GPIOA_AFRL = (GPIOA_AFRL & ~(0xffu << 8)) | (0x11u << 8);
    GPIOA_AFRH = (GPIOA_AFRH & ~0xfffu) | 0x110u;
    for (unsigned i = 0; i < 2; ++i) {
        USART_BAUD(ports[i]) = 625u; /* 72 MHz / 115200 */
        USART_CTL0(ports[i]) = (1u << 0) | (1u << 2) | (1u << 3);
    }
    SYST_RVR = 72000u - 1u; SYST_CVR = 0; SYST_CSR = 7u;
}

void platform_deinit(void)
{
    SYST_CSR = 0;
    for (unsigned i = 0; i < 8; ++i) {
        REG32(0xe000e180u + i * 4u) = 0xffffffffu;
        REG32(0xe000e280u + i * 4u) = 0xffffffffu;
    }
}
uint32_t platform_millis(void) { return ticks; }
int platform_uart_probe_byte(uint8_t *port, uint8_t *value)
{
    for (uint8_t i = 0; i < 2; ++i)
        if (USART_STAT(ports[i]) & (1u << 5)) {
            *port = i; *value = (uint8_t)USART_RDATA(ports[i]); return 1;
        }
    return 0;
}
int platform_uart_read(uint8_t port, uint8_t *value, uint32_t timeout_ms)
{
    uint32_t start = ticks;
    while ((uint32_t)(ticks - start) < timeout_ms)
        if (USART_STAT(ports[port]) & (1u << 5)) {
            *value = (uint8_t)USART_RDATA(ports[port]); return 1;
        }
    return 0;
}
void platform_uart_write(uint8_t port, uint8_t value)
{
    while (!(USART_STAT(ports[port]) & (1u << 7))) {}
    USART_TDATA(ports[port]) = value;
}
#define FMC_BASE 0x40022000u
#define FMC_WS REG32(FMC_BASE + 0x00u)
#define FMC_KEY REG32(FMC_BASE + 0x04u)
#define FMC_STAT REG32(FMC_BASE + 0x0cu)
#define FMC_CTL REG32(FMC_BASE + 0x10u)
#define FMC_ADDR REG32(FMC_BASE + 0x14u)
static uint8_t flash_page[1024];
static int flash_wait(void)
{
    uint32_t timeout = 0xf0000u;
    while ((FMC_STAT & 1u) && --timeout) {}
    if (!timeout) return -1;
    uint32_t error = FMC_STAT & ((1u << 2) | (1u << 3) | (1u << 4));
    FMC_STAT = (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5);
    return error ? -1 : 0;
}
int platform_flash_write(uint32_t address, const uint8_t *data, size_t length)
{
    if ((address & 1023u) || !length || length > 1024u) return -1;
    for (size_t i = 0; i < sizeof(flash_page); ++i)
        flash_page[i] = *(const volatile uint8_t *)(address + i);
    for (size_t i = 0; i < length; ++i) flash_page[i] = data[i];
    if (flash_wait()) return -1;
    if (FMC_CTL & (1u << 7)) {
        FMC_KEY = 0x45670123u; FMC_KEY = 0xcdef89abu;
        if (FMC_CTL & (1u << 7)) return -1;
    }
    FMC_CTL = 1u << 1; FMC_ADDR = address; FMC_CTL = (1u << 1) | (1u << 6);
    if (flash_wait()) goto fail;
    FMC_WS |= 1u << 15; FMC_CTL = 1u;
    for (size_t i = 0; i < sizeof(flash_page); i += 4) {
        uint32_t value = 0xffffffffu;
        size_t remain = sizeof(flash_page) - i;
        uint8_t *v = (uint8_t *)&value;
        for (size_t j = 0; j < 4 && j < remain; ++j) v[j] = flash_page[i + j];
        REG32(address + (uint32_t)i) = value;
        if (flash_wait()) goto fail;
    }
    FMC_CTL = 0; FMC_WS &= ~(1u << 15); FMC_CTL = 1u << 7;
    for (size_t i = 0; i < sizeof(flash_page); ++i)
        if (*(const volatile uint8_t *)(address + i) != flash_page[i]) return -1;
    return 0;
fail:
    FMC_CTL = 0; FMC_WS &= ~(1u << 15); FMC_CTL = 1u << 7; return -1;
}
void platform_reset(void) { SCB_AIRCR = 0x05fa0004u; for (;;) {} }
void platform_jump_to_app(void)
{
    uint32_t stack = REG32(APP_BASE), entry = REG32(APP_BASE + 4u);
    platform_deinit();
    __asm volatile("cpsid i\nmsr msp, %0\nbx %1" :: "r"(stack), "r"(entry));
    __builtin_unreachable();
}
