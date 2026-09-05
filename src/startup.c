#include <stdint.h>

extern uint32_t __stack_top__, __data_load__, __data_start__, __data_end__;
extern uint32_t __bss_start__, __bss_end__;
extern int main(void);
extern void SysTick_Handler(void);

void Default_Handler(void) { for (;;) {} }
void Reset_Handler(void)
{
    uint32_t *src = &__data_load__;
    for (uint32_t *dst = &__data_start__; dst < &__data_end__;)
        *dst++ = *src++;
    for (uint32_t *dst = &__bss_start__; dst < &__bss_end__;)
        *dst++ = 0;
    (void)main();
    for (;;) {}
}

__attribute__((section(".vectors"), used))
const uintptr_t vectors[16] = {
    [0] = (uintptr_t)&__stack_top__, [1] = (uintptr_t)Reset_Handler,
    [2 ... 14] = (uintptr_t)Default_Handler,
    [15] = (uintptr_t)SysTick_Handler,
};
