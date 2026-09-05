#include "platform.h"

#define REG32(a) (*(volatile uint32_t *)(a))
#define SYST_CSR REG32(0xe000e010u)
#define SYST_RVR REG32(0xe000e014u)
#define SYST_CVR REG32(0xe000e018u)
#define SCB_VTOR REG32(0xe000ed08u)
#define SCB_AIRCR REG32(0xe000ed0cu)
#define RCU_CTL REG32(0x40021000u)
#define RCU_CFG0 REG32(0x40021004u)
#define RCU_APB2EN REG32(0x40021018u)
#define RCU_APB1EN REG32(0x4002101cu)
#define PMU_CTL REG32(0x40007000u)
#define PMU_CS REG32(0x40007004u)
#define GPIOA_CTL0 REG32(0x40010800u)
#define GPIOA_CTL1 REG32(0x40010804u)
#define USART0_BASE 0x40013800u
#define USART1_BASE 0x40004400u
#define USART_STAT(b) REG32((b) + 0x00u)
#define USART_DATA(b) REG32((b) + 0x04u)
#define USART_BAUD(b) REG32((b) + 0x08u)
#define USART_CTL0(b) REG32((b) + 0x0cu)

static uint32_t ticks;
static const uint32_t ports[2] = { USART0_BASE, USART1_BASE };

void SysTick_Handler(void) { ++ticks; }

static int clock_setup(void)
{
    uint32_t timeout = 0xffffu;
    RCU_CTL |= 1u << 16;                 /* HXTAL enable */
    while (!(RCU_CTL & (1u << 17)) && --timeout) {}
    if (!timeout) return -1;

    RCU_APB1EN |= 1u << 28;              /* PMU clock */
    PMU_CTL |= 3u << 14;                 /* 1.3 V LDO */
    RCU_CFG0 &= ~((15u << 4) | (7u << 8) | (7u << 11)
                  | (1u << 16) | (1u << 17) | (15u << 18)
                  | (1u << 27) | (1u << 30));
    RCU_CFG0 |= (4u << 8) | (4u << 11); /* APB1/APB2 = AHB/2 */
    RCU_CFG0 |= (1u << 16) | (1u << 17) /* HXTAL/2 */
                | (13u << 18) | (1u << 27); /* x30 = 120 MHz */
    RCU_CTL |= 1u << 24;                 /* PLL enable */
    timeout = 0xfffffu;
    while (!(RCU_CTL & (1u << 25)) && --timeout) {}
    if (!timeout) return -1;
    PMU_CTL |= 1u << 16;                 /* high-drive enable */
    timeout = 0xfffffu;
    while (!(PMU_CS & (1u << 16)) && --timeout) {}
    if (!timeout) return -1;
    PMU_CTL |= 1u << 17;                 /* high-drive switch */
    timeout = 0xfffffu;
    while (!(PMU_CS & (1u << 17)) && --timeout) {}
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
    RCU_APB2EN |= (1u << 2) | (1u << 14); /* GPIOA, USART0 */
    RCU_APB1EN |= (1u << 17);             /* USART1 */
    GPIOA_CTL1 = (GPIOA_CTL1 & ~0xff0u) | 0x4b0u; /* PA9 TX, PA10 RX */
    GPIOA_CTL0 = (GPIOA_CTL0 & ~0xff00u) | 0x4b00u; /* PA2 TX, PA3 RX */
    for (unsigned i = 0; i < 2; ++i) {
        USART_BAUD(ports[i]) = 521u; /* 60 MHz peripheral clock / 115200 */
        USART_CTL0(ports[i]) = (1u << 13) | (1u << 3) | (1u << 2);
    }
    SYST_RVR = 120000u - 1u;
    SYST_CVR = 0;
    SYST_CSR = 7u;
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
            *port = i; *value = (uint8_t)USART_DATA(ports[i]); return 1;
        }
    return 0;
}

int platform_uart_read(uint8_t port, uint8_t *value, uint32_t timeout_ms)
{
    uint32_t start = ticks;
    while ((uint32_t)(ticks - start) < timeout_ms)
        if (USART_STAT(ports[port]) & (1u << 5)) {
            *value = (uint8_t)USART_DATA(ports[port]); return 1;
        }
    return 0;
}

void platform_uart_write(uint8_t port, uint8_t value)
{
    while (!(USART_STAT(ports[port]) & (1u << 7))) {}
    USART_DATA(ports[port]) = value;
}

#define FMC_BASE 0x40022000u
#define FMC_KEY0 REG32(FMC_BASE + 0x04u)
#define FMC_STAT0 REG32(FMC_BASE + 0x0cu)
#define FMC_CTL0 REG32(FMC_BASE + 0x10u)
#define FMC_ADDR0 REG32(FMC_BASE + 0x14u)
#define FMC_KEY1 REG32(FMC_BASE + 0x44u)
#define FMC_STAT1 REG32(FMC_BASE + 0x4cu)
#define FMC_CTL1 REG32(FMC_BASE + 0x50u)
#define FMC_ADDR1 REG32(FMC_BASE + 0x54u)
#define FLASH_DENSITY_KIB (*(const volatile uint16_t *)0x1ffff7e0u)
#define FLASH_BASE 0x08000000u
#define FLASH_BANK1_BASE 0x08080000u
static uint8_t flash_page[2048];
struct flash_bank {
    volatile uint32_t *key, *stat, *ctl, *addr;
};
static int flash_wait(volatile uint32_t *stat)
{
    uint32_t timeout = 0xf0000u;
    while ((*stat & 1u) && --timeout) {}
    if (!timeout) return -1;
    uint32_t error = *stat & ((1u << 2) | (1u << 4));
    *stat = (1u << 2) | (1u << 4) | (1u << 5);
    return error ? -1 : 0;
}
static int flash_select_bank(uint32_t address, struct flash_bank *bank)
{
    uint32_t flash_kib = FLASH_DENSITY_KIB;
    if (flash_kib < 2u || flash_kib > 1024u) return -1;
    uint32_t flash_end = FLASH_BASE + flash_kib * 1024u;
    if (address < FLASH_BASE || address > flash_end - sizeof(flash_page))
        return -1;
    if (address >= FLASH_BANK1_BASE) {
        if (flash_kib <= 512u) return -1;
        bank->key = &FMC_KEY1; bank->stat = &FMC_STAT1;
        bank->ctl = &FMC_CTL1; bank->addr = &FMC_ADDR1;
    } else {
        bank->key = &FMC_KEY0; bank->stat = &FMC_STAT0;
        bank->ctl = &FMC_CTL0; bank->addr = &FMC_ADDR0;
    }
    return 0;
}
int platform_flash_write(uint32_t address, const uint8_t *data, size_t length)
{
    struct flash_bank bank;
    if ((address & 2047u) || !length || length > 2048u) return -1;
    if (flash_select_bank(address, &bank)) return -1;
    /* The factory implementation preserves the unmodified tail of a short
     * final page across erase/program. */
    for (size_t i = 0; i < sizeof(flash_page); ++i)
        flash_page[i] = *(const volatile uint8_t *)(address + i);
    for (size_t i = 0; i < length; ++i) flash_page[i] = data[i];
    if (flash_wait(bank.stat)) return -1;
    if (*bank.ctl & (1u << 7)) {
        *bank.key = 0x45670123u; *bank.key = 0xcdef89abu;
        if (*bank.ctl & (1u << 7)) return -1;
    }
    *bank.ctl = 1u << 1; *bank.addr = address;
    *bank.ctl = (1u << 1) | (1u << 6);
    if (flash_wait(bank.stat)) goto fail;
    *bank.ctl = 1u;
    for (size_t i = 0; i < sizeof(flash_page); i += 2) {
        uint16_t value = flash_page[i];
        value |= (uint16_t)flash_page[i + 1] << 8;
        *(volatile uint16_t *)(address + i) = value;
        if (flash_wait(bank.stat)) goto fail;
    }
    *bank.ctl = 0; *bank.ctl = 1u << 7;
    for (size_t i = 0; i < sizeof(flash_page); ++i)
        if (*(const volatile uint8_t *)(address + i) != flash_page[i]) return -1;
    return 0;
fail:
    *bank.ctl = 0; *bank.ctl = 1u << 7; return -1;
}

void platform_reset(void) { SCB_AIRCR = 0x05fa0004u; for (;;) {} }

void platform_jump_to_app(void)
{
    uint32_t stack = REG32(APP_BASE);
    uint32_t entry = REG32(APP_BASE + 4u);
    platform_deinit();
    __asm volatile("cpsid i\nmsr msp, %0\nbx %1" :: "r"(stack), "r"(entry));
    __builtin_unreachable();
}
