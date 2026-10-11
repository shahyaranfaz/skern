import subprocess
import unittest

from common import ROOT, ensure_build


class KernelElfTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ensure_build()
        cls.kernel = ROOT / "build" / "kernel" / "kernel.elf"

    def test_kernel_is_x86_64_executable_at_expected_entry(self) -> None:
        header = subprocess.run(
            ["readelf", "-h", self.kernel], capture_output=True, text=True, check=True
        ).stdout

        self.assertRegex(header, r"Class:\s+ELF64")
        self.assertRegex(header, r"Data:\s+2's complement, little endian")
        self.assertRegex(header, r"Type:\s+EXEC")
        self.assertRegex(header, r"Machine:\s+Advanced Micro Devices X86-64")
        self.assertRegex(header, r"Entry point address:\s+0x200000")

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
        self.assertTrue(
            any(
                int(segment[2], 16) <= 0x200000 < int(segment[2], 16) + int(segment[5], 16)
                for segment in executable_segments
            )
        )


if __name__ == "__main__":
    unittest.main()
