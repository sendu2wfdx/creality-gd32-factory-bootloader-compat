from pathlib import Path
import struct


ROOT = Path(__file__).parents[1]


def test_all_images_fit_factory_region_and_have_valid_vectors():
    expected_stacks = {
        "c23": 0x2000C000,
        "c13": 0x20008000,
        "c10": 0x20002000,
    }
    for profile, expected_stack in expected_stacks.items():
        image = (ROOT / "build" / profile / "bootloader.bin").read_bytes()
        assert len(image) <= 0x3000
        stack, reset = struct.unpack_from("<II", image)
        assert stack == expected_stack
        assert reset & 1
        assert 0x08000000 <= (reset & ~1) < 0x08003000
        assert image[0x2F80:0x2F8C] == {
            "c23": b"mcu0_140_G31",
            "c13": b"noz0_110_G30",
            "c10": b"bed0_110_G21",
        }[profile]
