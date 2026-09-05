from hashlib import sha256
from pathlib import Path
import struct
import pytest


WORKSPACE = Path(__file__).parents[3]
DUMPS = (WORKSPACE / "factory-reference" / "machine-backup" /
         "c23-v1.1.0.53" / "GDFW_DUMP")

PROFILES = {
    "c23": {
        "file": "CR4NU200360C23_gd32f303_256K_backup_20260831.bin",
        "sha": "edb7438733bacf3e68fc20b0821bf423ac1810e2eb1403a7b56e8a3ee15eceff",
        "stack": 0x2000C000, "reset": 0x08000440,
        "board": b"mcu0_140_G31", "version": b"mcu0_022_000",
        "length": 35940, "crc": 0xB78A,
    },
    "c13": {
        "file": "CR1FN240306C13_gd32f303_128K_backup_20260831.bin",
        "sha": "c99a5edde286811a44dc85554b0d5fccc08017b476adeb02d5db6e1308ebda9b",
        "stack": 0x20008000, "reset": 0x0800047C,
        "board": b"noz0_110_G30", "version": b"noz0_019_000",
        "length": 30136, "crc": 0xF5DA,
    },
    "c10": {
        "file": "CR0NN200360C10_gd32e230_64K_backup_20260831.bin",
        "sha": "88bcb813f92ba1d62a22d3843cc0eb0f903e62164c862842152b143ff5546158",
        "stack": 0x20002000, "reset": 0x0800049C,
        "board": b"bed0_110_G21", "version": b"bed0_017_000",
        "length": 28616, "crc": 0x8C8E,
    },
}


def crc16(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def test_factory_images_anchor_recovered_contract():
    if not DUMPS.is_dir():
        pytest.skip("私有原厂完整备份未提供；公开 CI 不包含这些文件")
    for evidence in PROFILES.values():
        dump = (DUMPS / evidence["file"]).read_bytes()
        bootloader, app = dump[:0x3000], bytearray(dump[0x3000:])
        assert sha256(bootloader).hexdigest() == evidence["sha"]
        stack, reset = struct.unpack_from("<II", bootloader)
        assert stack == evidence["stack"]
        assert (reset & ~1) == evidence["reset"]
        assert bootloader[0x2F80:0x2F8C] == evidence["board"]
        assert app[0x200:0x20C] == evidence["version"]
        stored_crc, length = struct.unpack_from("<HI", app, 0x20C)
        assert (stored_crc, length) == (evidence["crc"], evidence["length"])
        app[0x20C:0x212] = b"\0" * 6
        assert crc16(app[:length]) == stored_crc
