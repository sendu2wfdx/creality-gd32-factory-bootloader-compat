#pragma once

#if defined(BOARD_C23) || defined(CONFIG_BOARD_C23)
#define BOARD_ID "mcu0_140_G31"
#ifdef CONFIG_F303_FLASH_SIZE_KIB
#define FLASH_SIZE (CONFIG_F303_FLASH_SIZE_KIB * 1024u)
#else
#define FLASH_SIZE (256u * 1024u)
#endif
#define BLOCK_SIZE 2048u
#define SECTOR_CODE 2u
#elif defined(BOARD_C13) || defined(CONFIG_BOARD_C13)
#define BOARD_ID "noz0_110_G30"
#ifdef CONFIG_F303_FLASH_SIZE_KIB
#define FLASH_SIZE (CONFIG_F303_FLASH_SIZE_KIB * 1024u)
#else
#define FLASH_SIZE (128u * 1024u)
#endif
#define BLOCK_SIZE 2048u
#define SECTOR_CODE 2u
#elif defined(BOARD_C10) || defined(CONFIG_BOARD_C10)
#define BOARD_ID "bed0_110_G21"
#define FLASH_SIZE (64u * 1024u)
#define BLOCK_SIZE 1024u
#define SECTOR_CODE 1u
#else
#error Select BOARD_C23, BOARD_C13, or BOARD_C10
#endif

#define MAX_APP_SIZE (FLASH_SIZE - 0x3000u)
#ifndef CONFIG_BOOT_WAIT_MS
#define CONFIG_BOOT_WAIT_MS 15000
#endif
#ifndef CONFIG_PACKET_TIMEOUT_MS
#define CONFIG_PACKET_TIMEOUT_MS 500
#endif
_Static_assert(sizeof(BOARD_ID) == 13, "board id must be exactly 12 bytes");
