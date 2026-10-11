import ctypes
import random
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from common import ROOT


ELF_HEADER = struct.Struct("<16sHHIQQQIHHHHHH")
PROGRAM_HEADER = struct.Struct("<IIQQQQQQ")


def make_elf(*, header=None, program=None) -> bytes:
    header_values = {
        "magic": b"\x7fELF",
        "elf_class": 2,
        "data": 1,
        "ident_version": 1,
        "os_abi": 0,
        "type": 2,
        "machine": 62,
        "version": 1,
        "entry": 0x200000,
        "program_offset": ELF_HEADER.size,
        "header_size": ELF_HEADER.size,
        "program_entry_size": PROGRAM_HEADER.size,
        "program_count": 1,
    }
    program_values = {
        "type": 1,
        "flags": 5,
        "offset": 0x1000,
        "virtual_address": 0x200000,
        "physical_address": 0x200000,
        "file_size": 1,
        "memory_size": 1,
        "alignment": 0x1000,
    }
    if header:
        header_values.update(header)
    if program:
        program_values.update(program)

    ident = bytearray(16)
    ident[:4] = header_values["magic"]
    ident[4] = header_values["elf_class"]
    ident[5] = header_values["data"]
    ident[6] = header_values["ident_version"]
    ident[7] = header_values["os_abi"]

    elf_header = ELF_HEADER.pack(
        bytes(ident),
        header_values["type"],
        header_values["machine"],
        header_values["version"],
        header_values["entry"],
        header_values["program_offset"],
        0,
        0,
        header_values["header_size"],
        header_values["program_entry_size"],
        header_values["program_count"],
        0,
        0,
        0,
    )
    program_header = PROGRAM_HEADER.pack(
        program_values["type"],
        program_values["flags"],
        program_values["offset"],
        program_values["virtual_address"],
        program_values["physical_address"],
        program_values["file_size"],
        program_values["memory_size"],
        program_values["alignment"],
    )

    file_size = max(
        len(elf_header),
        header_values["program_offset"] + len(program_header),
        program_values["offset"] + program_values["file_size"],
    )
    image = bytearray(file_size)
    image[: len(elf_header)] = elf_header
    start = header_values["program_offset"]
    image[start : start + len(program_header)] = program_header
    return bytes(image)


class ElfValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temporary_directory = tempfile.TemporaryDirectory()
        library = Path(cls.temporary_directory.name) / "libelf_validate.so"
        subprocess.run(
            [
                "cc",
                "-shared",
                "-fPIC",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(ROOT / "boot"),
                str(ROOT / "boot" / "elf_validate.c"),
                "-o",
                str(library),
            ],
            check=True,
        )
        cls.library = ctypes.CDLL(str(library))
        cls.library.elf_validate.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        cls.library.elf_validate.restype = ctypes.c_bool

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary_directory.cleanup()

    def validate(self, data: bytes) -> bool:
        buffer = ctypes.create_string_buffer(data, len(data))
        return self.library.elf_validate(buffer, len(data))

    def test_accepts_valid_executable(self) -> None:
        self.assertTrue(self.validate(make_elf()))

    def test_rejects_invalid_elf_identity(self) -> None:
        cases = (
            {"magic": b"BAD!"},
            {"elf_class": 1},
            {"data": 2},
            {"ident_version": 0},
            {"os_abi": 3},
            {"type": 3},
            {"machine": 3},
            {"version": 0},
        )
        for values in cases:
            with self.subTest(values=values):
                self.assertFalse(self.validate(make_elf(header=values)))

    def test_rejects_truncated_headers(self) -> None:
        valid = make_elf()
        self.assertFalse(self.validate(valid[: ELF_HEADER.size - 1]))
        self.assertFalse(self.validate(valid[: ELF_HEADER.size + PROGRAM_HEADER.size - 1]))

    def test_rejects_invalid_segment_ranges(self) -> None:
        cases = (
            {"file_size": 2, "memory_size": 1},
            {"virtual_address": 0xFFFFFFFFFFFFFFFE, "memory_size": 4},
            {"physical_address": 0xFFFFFFFFFFFFFFFE, "memory_size": 4},
        )
        for values in cases:
            with self.subTest(values=values):
                self.assertFalse(self.validate(make_elf(program=values)))

    def test_rejects_invalid_segment_alignment(self) -> None:
        self.assertFalse(self.validate(make_elf(program={"alignment": 3})))
        self.assertFalse(
            self.validate(make_elf(program={"offset": 0x1001, "alignment": 0x1000}))
        )

    def test_rejects_dynamic_or_interpreted_images(self) -> None:
        self.assertFalse(self.validate(make_elf(program={"type": 2})))
        self.assertFalse(self.validate(make_elf(program={"type": 3})))

    def test_rejects_entry_outside_executable_segment(self) -> None:
        self.assertFalse(self.validate(make_elf(header={"entry": 0x201000})))
        self.assertFalse(self.validate(make_elf(program={"flags": 4})))

    def test_rejects_random_bytes_without_crashing(self) -> None:
        generator = random.Random(0x5A3E7)
        for size in range(256):
            data = generator.randbytes(size)
            self.assertFalse(self.validate(data), f"accepted random input of size {size}")


if __name__ == "__main__":
    unittest.main()
