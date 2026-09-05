CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
PYTHON ?= python3
HOSTCC ?= cc
LOCAL_KCONFIGLIB := ../Katapult_GD32/lib/kconfiglib
KCONFIGLIB ?= $(if $(wildcard $(LOCAL_KCONFIGLIB)/kconfiglib.py),$(LOCAL_KCONFIGLIB),$(shell $(PYTHON) -c "import os,kconfiglib; print(os.path.dirname(kconfiglib.__file__))" 2>/dev/null))
KCONFIG_CONFIG ?= .config

ifeq ($(filter clean distclean,$(MAKECMDGOALS)),)
-include $(KCONFIG_CONFIG)
endif

ifeq ($(CONFIG_BOARD_C10),y)
CONFIG_CPU := cortex-m23
CONFIG_PLATFORM := src/platform_gd32e230.c
CONFIG_RAM_SIZE := 8192
else ifeq ($(CONFIG_BOARD_C13),y)
CONFIG_CPU := cortex-m3
CONFIG_PLATFORM := src/platform_gd32f303.c
CONFIG_RAM_SIZE := 32768
else
CONFIG_CPU := cortex-m3
CONFIG_PLATFORM := src/platform_gd32f303.c
CONFIG_RAM_SIZE := 49152
endif

CFLAGS := -std=c11 -Os -ffreestanding -fno-builtin -ffunction-sections \
	-fdata-sections -Wall -Wextra -Werror -Isrc
LDFLAGS = -nostdlib -Wl,--gc-sections -Wl,-Map,$@.map -T linker.ld -lgcc
COMMON := src/startup.c src/protocol.c src/main.c

.PHONY: all configured profiles hosttest clean distclean c23 c13 c10 menuconfig olddefconfig
all: configured
configured: build/configured/bootloader.bin
profiles: c23 c13 c10
hosttest: build/host-tests/protocol-c23 build/host-tests/protocol-c13 \
		build/host-tests/protocol-c10
	build/host-tests/protocol-c23
	build/host-tests/protocol-c13
	build/host-tests/protocol-c10

$(KCONFIG_CONFIG): Kconfig
	@test -n "$(KCONFIGLIB)" || (echo "缺少 Kconfiglib；请执行: python3 -m pip install kconfiglib" && false)
	$(PYTHON) $(KCONFIGLIB)/olddefconfig.py Kconfig

olddefconfig: Kconfig
	@test -n "$(KCONFIGLIB)" || (echo "缺少 Kconfiglib；请执行: python3 -m pip install kconfiglib" && false)
	$(PYTHON) $(KCONFIGLIB)/olddefconfig.py Kconfig

menuconfig: Kconfig
	@test -n "$(KCONFIGLIB)" || (echo "缺少 Kconfiglib；请执行: python3 -m pip install kconfiglib" && false)
	$(PYTHON) $(KCONFIGLIB)/menuconfig.py Kconfig

build/configured/autoconf.h: $(KCONFIG_CONFIG) Kconfig | build/configured
	@test -n "$(KCONFIGLIB)" || (echo "缺少 Kconfiglib；请执行: python3 -m pip install kconfiglib" && false)
	KCONFIG_CONFIG=$(KCONFIG_CONFIG) KCONFIG_AUTOHEADER=$@ \
		$(PYTHON) $(KCONFIGLIB)/genconfig.py Kconfig

build/configured/bootloader.elf: $(COMMON) $(CONFIG_PLATFORM) build/configured/autoconf.h
	$(CC) $(CFLAGS) -include build/configured/autoconf.h \
		-mcpu=$(CONFIG_CPU) -mthumb $(COMMON) $(CONFIG_PLATFORM) $(LDFLAGS) \
		-Wl,--defsym=RAM_SIZE=$(CONFIG_RAM_SIZE) -o $@
	$(SIZE) $@

c23: build/c23/bootloader.bin
c13: build/c13/bootloader.bin
c10: build/c10/bootloader.bin

build/c23/bootloader.elf: $(COMMON) src/platform_gd32f303.c | build/c23
	$(CC) $(CFLAGS) -mcpu=cortex-m3 -mthumb -DBOARD_C23 $^ $(LDFLAGS) \
		-Wl,--defsym=RAM_SIZE=49152 -o $@
	$(SIZE) $@

build/c13/bootloader.elf: $(COMMON) src/platform_gd32f303.c | build/c13
	$(CC) $(CFLAGS) -mcpu=cortex-m3 -mthumb -DBOARD_C13 $^ $(LDFLAGS) \
		-Wl,--defsym=RAM_SIZE=32768 -o $@
	$(SIZE) $@

build/c10/bootloader.elf: $(COMMON) src/platform_gd32e230.c | build/c10
	$(CC) $(CFLAGS) -mcpu=cortex-m23 -mthumb -DBOARD_C10 $^ $(LDFLAGS) \
		-Wl,--defsym=RAM_SIZE=8192 -o $@
	$(SIZE) $@

build/%/bootloader.bin: build/%/bootloader.elf
	$(OBJCOPY) -O binary $< $@

build/host-tests/protocol-c23: src/protocol.c tests/protocol_host_harness.c | build/host-tests
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -DBOARD_C23 -Isrc $^ -o $@

build/host-tests/protocol-c13: src/protocol.c tests/protocol_host_harness.c | build/host-tests
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -DBOARD_C13 -Isrc $^ -o $@

build/host-tests/protocol-c10: src/protocol.c tests/protocol_host_harness.c | build/host-tests
	$(HOSTCC) -std=c11 -O2 -Wall -Wextra -Werror -DBOARD_C10 -Isrc $^ -o $@

build/configured build/c23 build/c13 build/c10 build/host-tests:
	mkdir -p $@

clean:
	rm -rf build

distclean: clean
	rm -f $(KCONFIG_CONFIG)
