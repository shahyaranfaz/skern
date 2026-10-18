from pathlib import Path
import subprocess
import unittest

from common import ROOT


EXECUTABLE = ROOT / "build" / "tests" / "console_test"
CASES = (
    "framebuffer-invalid",
    "framebuffer-colors",
    "framebuffer-rows",
    "console-uninitialized",
    "console-render-wrap",
    "console-controls",
    "console-scroll",
)


class ConsoleTests(unittest.TestCase):
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
                "tests/console_test.cpp",
                "kernel/console/console.cpp",
                "kernel/console/framebuffer.cpp",
                "-o",
                str(EXECUTABLE),
            ],
            cwd=ROOT,
            check=True,
        )


def add_console_test(case: str) -> None:
    def test(self: ConsoleTests) -> None:
        result = subprocess.run(
            [str(EXECUTABLE), case],
            cwd=ROOT,
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    setattr(ConsoleTests, f"test_{case.replace('-', '_')}", test)


for case in CASES:
    add_console_test(case)


if __name__ == "__main__":
    unittest.main()
