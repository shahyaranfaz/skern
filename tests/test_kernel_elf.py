import re
import subprocess
import unittest

from common import ROOT, ensure_build


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

    def test_load_segments_use_matching_physical_and_virtual_addresses(self) -> None:
        program_headers = subprocess.run(
            ["readelf", "-W", "-l", self.kernel], capture_output=True, text=True, check=True
        ).stdout
        load_segments = [
            line.split() for line in program_headers.splitlines() if line.lstrip().startswith("LOAD")
        ]

        self.assertGreater(len(load_segments), 0)
        for segment in load_segments:
            self.assertEqual(segment[2], segment[3])

        executable_segments = [segment for segment in load_segments if "E" in segment[6:-1]]
        self.assertTrue(executable_segments)
        entry = self.kernel_main_address()
        self.assertTrue(any(
            int(segment[2], 16) <= entry < int(segment[2], 16) + int(segment[5], 16)
            for segment in executable_segments
        ))


if __name__ == "__main__":
    unittest.main()
