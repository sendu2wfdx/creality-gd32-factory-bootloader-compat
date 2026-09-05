from pathlib import Path
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).parents[1]


def test_compiled_protocol_state_machine_with_mocked_uart_and_flash():
    cc = shutil.which("cc")
    if cc is None:
        import pytest
        pytest.skip("host C compiler is unavailable")
    for profile in ("C23", "C13", "C10"):
        with tempfile.TemporaryDirectory(
                prefix=f"creality-bl-{profile.lower()}-") as temp_dir:
            executable = Path(temp_dir) / "protocol-host-test"
            compile_result = subprocess.run([
                cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                f"-DBOARD_{profile}", "-Isrc", "src/protocol.c",
                "tests/protocol_host_harness.c", "-o", os.fspath(executable),
            ], cwd=ROOT, text=True, capture_output=True)
            assert compile_result.returncode == 0, compile_result.stderr
            run_result = subprocess.run(
                [os.fspath(executable)], cwd=ROOT, text=True, capture_output=True)
            assert run_result.returncode == 0, run_result.stderr
            assert run_result.stdout.strip() == \
                "factory_bootloader_protocol_host=12/12"
