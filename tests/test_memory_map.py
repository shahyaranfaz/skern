from pathlib import Path
import subprocess
import unittest

from common import ROOT


EXECUTABLE = ROOT / "build" / "tests" / "memory_map_test"
CASES = ("sort-merge", "types", "overlap", "malformed")


class MemoryMapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        EXECUTABLE.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(
            [
                "g++",
                "-std=c++20",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Iinclude",
                "-Ikernel",
                "tests/memory_map_test.cpp",
                "kernel/mm/memory_map.cpp",
                "-o",
                str(EXECUTABLE),
            ],
            cwd=ROOT,
            check=True,
        )


def add_memory_map_test(case: str) -> None:
    def test(self: MemoryMapTests) -> None:
        result = subprocess.run([str(EXECUTABLE), case], cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    setattr(MemoryMapTests, f"test_{case.replace('-', '_')}", test)


for case in CASES:
    add_memory_map_test(case)


if __name__ == "__main__":
    unittest.main()
