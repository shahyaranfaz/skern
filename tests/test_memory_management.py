import os
from pathlib import Path
import selectors
import shutil
import subprocess
import tempfile
import time
import unittest

from common import ROOT, ensure_build
from test_boot import OVMF_CODE_CANDIDATES, OVMF_VARS_CANDIDATES, find_firmware
from test_traps import KERNEL_CXXFLAGS


class MemoryManagementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ensure_build()
        qemu = shutil.which("qemu-system-x86_64")
        ovmf_code = find_firmware(OVMF_CODE_CANDIDATES)
        ovmf_vars = find_firmware(OVMF_VARS_CANDIDATES)
        if qemu is None or ovmf_code is None or ovmf_vars is None:
            raise unittest.SkipTest("QEMU and OVMF are required for heap tests")

        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            main_object = directory / "heap_kernel.o"
            kernel = directory / "kernel.elf"

            subprocess.run(
                [
                    "g++",
                    "-Iinclude",
                    "-Ikernel",
                    *KERNEL_CXXFLAGS,
                    "-c",
                    "tests/memory_management_kernel.cpp",
                    "-o",
                    str(main_object),
                ],
                cwd=ROOT,
                check=True,
            )

            sources = [
                path
                for pattern in ("*.cpp", "*.S")
                for path in sorted((ROOT / "kernel").rglob(pattern))
                if path != ROOT / "kernel" / "main.cpp"
            ]
            objects = [
                ROOT / "build" / source.relative_to(ROOT).parent / f"{source.name}.o"
                for source in sources
            ]
            objects.append(ROOT / "build" / "kernel" / "console" / "skernfont.o")
            subprocess.run(
                [
                    "ld",
                    "-nostdlib",
                    "-static",
                    "--build-id=none",
                    "-z",
                    "max-page-size=0x1000",
                    "-z",
                    "noexecstack",
                    "-T",
                    "kernel/linker.ld",
                    *map(str, objects),
                    str(main_object),
                    "-o",
                    str(kernel),
                ],
                cwd=ROOT,
                check=True,
            )

            esp = directory / "esp"
            shutil.copytree(ROOT / "build" / "esp", esp)
            shutil.copyfile(kernel, esp / "skern" / "kernel.elf")
            writable_vars = directory / "OVMF_VARS.fd"
            shutil.copyfile(ovmf_vars, writable_vars)

            process = subprocess.Popen(
                [
                    qemu,
                    "-machine",
                    "q35",
                    "-m",
                    "64M",
                    "-display",
                    "none",
                    "-monitor",
                    "none",
                    "-serial",
                    "stdio",
                    "-no-reboot",
                    "-drive",
                    f"if=pflash,format=raw,readonly=on,file={ovmf_code}",
                    "-drive",
                    f"if=pflash,format=raw,file={writable_vars}",
                    "-drive",
                    f"format=raw,file=fat:rw:{esp}",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
            )
            output = bytearray()
            selector = selectors.DefaultSelector()
            if process.stdout is None:
                process.terminate()
                raise RuntimeError("QEMU stdout pipe was not created")
            selector.register(process.stdout, selectors.EVENT_READ)
            deadline = time.monotonic() + 15

            try:
                while time.monotonic() < deadline and b"HEAP TESTS PASSED" not in output:
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

            cls.output = output.decode(errors="replace")

    def test_zero_size_request(self) -> None:
        self.assertIn("HEAP ZERO PASS", self.output)

    def test_multi_page_growth(self) -> None:
        self.assertIn("HEAP GROWTH PASS", self.output)

    def test_alignment_and_separation(self) -> None:
        self.assertIn("HEAP ALIGNMENT PASS", self.output)

    def test_reuses_released_blocks(self) -> None:
        self.assertIn("HEAP REUSE PASS", self.output)

    def test_coalesces_adjacent_blocks(self) -> None:
        self.assertIn("HEAP COALESCE PASS", self.output)

    def test_physical_frame_allocation(self) -> None:
        self.assertIn("PMM ALLOCATION PASS", self.output)

    def test_protected_ranges(self) -> None:
        self.assertIn("PMM PROTECTION PASS", self.output)

    def test_physical_frame_exhaustion(self) -> None:
        self.assertIn("PMM EXHAUSTION PASS", self.output)

    def test_virtual_mapping_permissions(self) -> None:
        self.assertIn("VMM MAPPING PASS", self.output)

    def test_tlb_invalidation(self) -> None:
        self.assertIn("VMM TLB PASS", self.output)


if __name__ == "__main__":
    unittest.main()
