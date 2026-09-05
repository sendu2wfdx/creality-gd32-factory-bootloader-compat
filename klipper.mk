# Optional integration with the Ender-3 V4 Klipper source tree.
# This file is included by the parent Klipper Makefile after its board config.

ifeq ($(CONFIG_BOARD_DIRECTORY),"gd32")

bootloader_src-y += src/startup.c src/protocol.c src/main.c
bootloader_dirs-y += bootloader/src

BOOTLOADER_CFLAGS += -Isrc/bootloader/src -ffreestanding -fno-builtin
BOOTLOADER_LINK_FLAGS := -nostdlib -Wl,--gc-sections \
    -Wl,-Map,$(OUT)bootloader.map -T src/bootloader/linker.ld -lgcc

ifeq ($(CONFIG_MAIN_MCU_BOARD),y)
bootloader_src-y += src/platform_gd32f303.c
BOOTLOADER_BOARD_FLAGS := -mcpu=cortex-m3 -mthumb -DBOARD_C23
BOOTLOADER_RAM_FLAGS := -Wl,--defsym=RAM_SIZE=49152
else ifeq ($(CONFIG_NOZZLE_MCU_BOARD),y)
bootloader_src-y += src/platform_gd32f303.c
BOOTLOADER_BOARD_FLAGS := -mcpu=cortex-m3 -mthumb -DBOARD_C13
BOOTLOADER_RAM_FLAGS := -Wl,--defsym=RAM_SIZE=32768
else ifeq ($(CONFIG_BED_MCU_BOARD),y)
bootloader_src-y += src/platform_gd32e230.c
BOOTLOADER_BOARD_FLAGS := -mcpu=cortex-m23 -mthumb -DBOARD_C10
BOOTLOADER_RAM_FLAGS := -Wl,--defsym=RAM_SIZE=8192
else
$(error Bootloader integration requires a main, nozzle, or bed MCU role)
endif

BOOTLOADER_CFLAGS += $(BOOTLOADER_BOARD_FLAGS)
CFLAGS_bootloader.elf += $(BOOTLOADER_BOARD_FLAGS) \
    $(BOOTLOADER_LINK_FLAGS) $(BOOTLOADER_RAM_FLAGS)

target-$(CONFIG_BOARD_INFO_CONFIGURE) += $(OUT)bootloader.bin

$(OUT)bootloader.bin: $(OUT)bootloader.elf
	@echo "  Creating $@"
	$(Q)$(OBJCOPY) -O binary $< $@

endif
