import subprocess
import unittest

from common import ROOT


SOURCE = r"""
#include <stddef.h>
#include <skern/boot_info.h>

#if defined(__cplusplus)
#define ABI_ASSERT static_assert
#else
#define ABI_ASSERT _Static_assert
#endif

ABI_ASSERT(sizeof(BootstrapStackInfo) == 16, "BootstrapStackInfo size");
ABI_ASSERT(sizeof(FramebufferInfo) == 32, "FramebufferInfo size");
ABI_ASSERT(sizeof(MemoryMapInfo) == 32, "MemoryMapInfo size");
ABI_ASSERT(sizeof(BootInfo) == 104, "BootInfo size");

ABI_ASSERT(offsetof(BootInfo, memory_map) == 16, "BootInfo memory_map offset");
ABI_ASSERT(offsetof(BootInfo, framebuffer) == 48, "BootInfo framebuffer offset");
ABI_ASSERT(offsetof(BootInfo, bootstrap_stack) == 80, "BootInfo bootstrap_stack offset");
ABI_ASSERT(offsetof(BootInfo, rsdp) == 96, "BootInfo rsdp offset");
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
        self.compile_header("cc", "c", "c11")

    def test_layout_is_stable_in_cpp(self) -> None:
        self.compile_header("c++", "c++", "c++20")


if __name__ == "__main__":
    unittest.main()
