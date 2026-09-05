from pathlib import Path
import re


ROOT = Path(__file__).parents[1]
SOURCE = (ROOT / "src" / "protocol.c").read_text(encoding="utf-8")
F303 = (ROOT / "src" / "platform_gd32f303.c").read_text(encoding="utf-8")


def checksum(data: bytes) -> int:
    return 0xFF - (sum(data) & 0xFF)


def crc16(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def test_wire_golden_values():
    assert checksum(b"\x00") == 0xFF
    assert checksum(b"\x01") == 0xFE
    assert checksum(b"\x02") == 0xFD
    assert checksum(b"\x03") == 0xFC
    assert checksum(b"\x75") == 0x8A


def test_crc_known_vector():
    assert crc16(b"123456789") == 0x31C3


def test_source_keeps_safety_capacity_check_and_retry_limits():
    assert "length > MAX_APP_SIZE" in SOURCE
    assert re.search(r"attempt < 5", SOURCE)
    assert re.search(r"attempt < 3", SOURCE)
    assert "The factory host rewinds" in SOURCE
    assert "if (!bl_application_valid())" in SOURCE


def test_f303_selects_second_flash_bank_from_factory_density_word():
    assert "0x1ffff7e0u" in F303
    assert "FLASH_BANK1_BASE 0x08080000u" in F303
    assert "flash_kib <= 512u" in F303
    for register_offset in ("0x44u", "0x4cu", "0x50u", "0x54u"):
        assert register_offset in F303
