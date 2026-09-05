from pathlib import Path


ROOT = Path(__file__).parents[1]


def test_menuconfig_covers_all_factory_profiles_and_timings():
    kconfig = (ROOT / "Kconfig").read_text(encoding="utf-8")
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    board = (ROOT / "src" / "board.h").read_text(encoding="utf-8")
    main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
    protocol = (ROOT / "src" / "protocol.c").read_text(encoding="utf-8")
    for symbol in ("BOARD_C23", "BOARD_C13", "BOARD_C10",
                   "BOOT_WAIT_MS", "PACKET_TIMEOUT_MS"):
        assert f"config {symbol}" in kconfig
    assert "config F303_FLASH_SIZE_KIB" in kconfig
    assert "menuconfig:" in makefile
    assert "CONFIG_BOARD_C10" in board
    assert "CONFIG_BOOT_WAIT_MS" in main
    assert "CONFIG_PACKET_TIMEOUT_MS" in protocol
