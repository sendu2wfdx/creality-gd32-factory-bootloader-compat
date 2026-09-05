MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 12K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = RAM_SIZE
}

ENTRY(Reset_Handler)

SECTIONS
{
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text : { *(.text*) *(.rodata*) } > FLASH
    .board_id 0x08002f80 : { KEEP(*(.board_id)) } > FLASH
    ASSERT(SIZEOF(.board_id) == 12,
           "factory board id must be exactly 12 bytes")
    .data : { __data_start__ = .; *(.data*) __data_end__ = .; } > RAM AT> FLASH
    __data_load__ = LOADADDR(.data);
    .bss (NOLOAD) : { __bss_start__ = .; *(.bss*) *(COMMON) __bss_end__ = .; } > RAM
    . = ALIGN(8);
    __stack_top__ = ORIGIN(RAM) + LENGTH(RAM);
    ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(FLASH) + LENGTH(FLASH),
           "bootloader exceeds original 12 KiB region")
}
