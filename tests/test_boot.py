import os
from pathlib import Path
import selectors
import shutil
import subprocess
import tempfile
import time
import unittest

from common import ROOT, ensure_build


OVMF_CODE_CANDIDATES = (
    "/usr/share/OVMF/OVMF_CODE_4M.fd",
    "/usr/share/OVMF/OVMF_CODE.fd",
    "/usr/share/edk2/x64/OVMF_CODE.fd",
    "/usr/share/edk2/ovmf/OVMF_CODE.fd",
)
OVMF_VARS_CANDIDATES = (
    "/usr/share/OVMF/OVMF_VARS_4M.fd",
    "/usr/share/OVMF/OVMF_VARS.fd",
    "/usr/share/edk2/x64/OVMF_VARS.fd",
    "/usr/share/edk2/ovmf/OVMF_VARS.fd",
)


def find_firmware(candidates: tuple[str, ...]) -> Path | None:
    return next((Path(candidate) for candidate in candidates if Path(candidate).is_file()), None)


class BootSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ensure_build()

    def test_bootloader_reaches_kernel(self) -> None:
        qemu = shutil.which("qemu-system-x86_64")
        ovmf_code = find_firmware(OVMF_CODE_CANDIDATES)
        ovmf_vars = find_firmware(OVMF_VARS_CANDIDATES)
        if qemu is None or ovmf_code is None or ovmf_vars is None:
            self.skipTest("QEMU and OVMF are required for the boot smoke test")

        with tempfile.TemporaryDirectory() as temporary_directory:
            writable_vars = Path(temporary_directory) / "OVMF_VARS.fd"
            shutil.copyfile(ovmf_vars, writable_vars)
            command = [
                qemu,
                "-machine",
                "q35",
                "-m",
                "256M",
                "-display",
                "none",
                "-monitor",
                "none",
                "-serial",
                "none",
                "-no-reboot",
                "-debugcon",
                "stdio",
                "-global",
                "isa-debugcon.iobase=0xe9",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={writable_vars}",
                "-drive",
                f"format=raw,file=fat:rw:{ROOT / 'build' / 'esp'}",
            ]
            process = subprocess.Popen(
                command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0
            )
            output = bytearray()
            selector = selectors.DefaultSelector()
            self.assertIsNotNone(process.stdout)
            selector.register(process.stdout, selectors.EVENT_READ)
            deadline = time.monotonic() + 15

            try:
                while time.monotonic() < deadline and b"Hello from skern!" not in output:
                    if process.poll() is not None:
                        output.extend(process.stdout.read())
                        break
                    for key, _ in selector.select(timeout=0.25):
                        output.extend(os.read(key.fd, 4096))
            finally:
                selector.close()
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
                process.stdout.close()

            self.assertIn(
                b"Hello from skern!",
                output,
                f"kernel message not observed; QEMU output: {output.decode(errors='replace')}",
            )


if __name__ == "__main__":
    unittest.main()
