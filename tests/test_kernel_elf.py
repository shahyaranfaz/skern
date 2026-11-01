import re
import subprocess
import unittest

from common import ROOT, ensure_build

PAGE_SIZE = 4096
SKERN_PHYS_BASE = 0x0000000000200000
SKERN_VIRT_BASE = 0xFFFFFFFF80000000


class KernelElfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ensure_build()
        cls.kernel = ROOT / "build" / "kernel" / "kernel.elf"

    def kernel_main_address(self) -> int:
        symbols = subprocess.run(
            ["nm", "-n", self.kernel], capture_output=True, text=True, check=True
        ).stdout
        for line in symbols.splitlines():
            address, _, name = line.split()
            if name == "kernel_main":
                return int(address, 16)
        self.fail("kernel_main symbol not found")

    def test_kernel_bounds_are_exported_and_page_aligned(self) -> None:
        symbols = subprocess.run(
            ["nm", "-n", self.kernel], capture_output=True, text=True, check=True
        ).stdout
        addresses = {
            name: int(address, 16)
            for line in symbols.splitlines()
            if len(line.split()) == 3
            for address, _, name in [line.split()]
            if name
            in {
                "__kernel_start",
                "__kernel_end",
                "__kernel_phys_start",
                "__kernel_phys_end",
            }
        }

        self.assertEqual(
            set(addresses),
            {
                "__kernel_start",
                "__kernel_end",
                "__kernel_phys_start",
                "__kernel_phys_end",
            },
        )
        self.assertEqual(addresses["__kernel_start"], SKERN_VIRT_BASE)
        self.assertEqual(addresses["__kernel_phys_start"], SKERN_PHYS_BASE)
        self.assertGreater(addresses["__kernel_end"], addresses["__kernel_start"])
        self.assertGreater(addresses["__kernel_phys_end"], addresses["__kernel_phys_start"])
        self.assertEqual(
            addresses["__kernel_end"] - addresses["__kernel_start"],
            addresses["__kernel_phys_end"] - addresses["__kernel_phys_start"],
        )
        for address in addresses.values():
            self.assertEqual(address % PAGE_SIZE, 0)

    def test_embedded_font_is_in_read_only_allocated_section(self) -> None:
        sections = subprocess.run(
            ["readelf", "-W", "-S", self.kernel], capture_output=True, text=True, check=True
        ).stdout
        font_section = next(
            (line.split() for line in sections.splitlines() if ".rodata.skernfont" in line), None
        )

        self.assertIsNotNone(font_section)
        flags = font_section[8]
        self.assertIn("A", flags)
        self.assertNotIn("W", flags)

    def test_kernel_is_x86_64_executable_at_expected_entry(self) -> None:
        header = subprocess.run(
            ["readelf", "-h", self.kernel], capture_output=True, text=True, check=True
        ).stdout

        self.assertRegex(header, r"Class:\s+ELF64")
        self.assertRegex(header, r"Data:\s+2's complement, little endian")
        self.assertRegex(header, r"Type:\s+EXEC")
        self.assertRegex(header, r"Machine:\s+Advanced Micro Devices X86-64")
        entry_match = re.search(r"Entry point address:\s+(0x[0-9a-fA-F]+)", header)
        self.assertIsNotNone(entry_match)
        self.assertEqual(int(entry_match.group(1), 16), self.kernel_main_address())

    def test_load_segments_use_high_virtual_and_low_physical_addresses(self) -> None:
        program_headers = subprocess.run(
            ["readelf", "-W", "-l", self.kernel], capture_output=True, text=True, check=True
        ).stdout
        load_segments = [
            line.split() for line in program_headers.splitlines() if line.lstrip().startswith("LOAD")
        ]

        self.assertGreater(len(load_segments), 0)
        for segment in load_segments:
            virtual_address = int(segment[2], 16)
            physical_address = int(segment[3], 16)

            self.assertGreaterEqual(virtual_address, SKERN_VIRT_BASE)
            self.assertGreaterEqual(physical_address, SKERN_PHYS_BASE)
            self.assertEqual(
                virtual_address - SKERN_VIRT_BASE,
                physical_address - SKERN_PHYS_BASE,
            )

        executable_segments = [segment for segment in load_segments if "E" in segment[6:-1]]
        self.assertTrue(executable_segments)
        entry = self.kernel_main_address()
        self.assertTrue(any(
            int(segment[2], 16) <= entry < int(segment[2], 16) + int(segment[5], 16)
            for segment in executable_segments
        ))


if __name__ == "__main__":
    unittest.main()
