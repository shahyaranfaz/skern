import subprocess
import unittest

from common import ROOT


SOURCE = r"""
#include <stddef.h>
#include <skern/boot_info.h>

static_assert(sizeof(BootstrapStackInfo) == 16);
static_assert(sizeof(FramebufferInfo) == 32);
static_assert(sizeof(MemoryMapInfo) == 32);
static_assert(sizeof(BootInfo) == 104);

static_assert(offsetof(BootInfo, memory_map) == 16);
static_assert(offsetof(BootInfo, framebuffer) == 48);
static_assert(offsetof(BootInfo, bootstrap_stack) == 80);
static_assert(offsetof(BootInfo, rsdp) == 96);
"""


class BootInfoAbiTests(unittest.TestCase):
    def compile_header(self, compiler: str, language: str, standard: str) -> None:
        subprocess.run(
            [
                compiler,
                f"-std={standard}",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "include"),
                "-x",
                language,
                "-fsyntax-only",
                "-",
            ],
            input=SOURCE,
            text=True,
            check=True,
        )

    def test_layout_is_stable_in_c(self) -> None:
        self.compile_header("cc", "c", "gnu2x")

    def test_layout_is_stable_in_cpp(self) -> None:
        self.compile_header("c++", "c++", "c++20")


if __name__ == "__main__":
    unittest.main()
