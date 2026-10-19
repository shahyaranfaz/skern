import os
from pathlib import Path
import re
import selectors
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

from common import ROOT, ensure_build
from test_boot import OVMF_CODE_CANDIDATES, OVMF_VARS_CANDIDATES, find_firmware


KERNEL_CXXFLAGS = (
    "-std=c++20",
    "-ffreestanding",
    "-fno-exceptions",
    "-fno-rtti",
    "-fno-threadsafe-statics",
    "-fno-use-cxa-atexit",
    "-fno-stack-protector",
    "-fno-stack-check",
    "-fno-pic",
    "-fno-pie",
    "-m64",
    "-mno-red-zone",
    "-mcmodel=kernel",
    "-mno-mmx",
    "-mno-sse",
    "-mno-sse2",
    "-Wall",
    "-Wextra",
    "-Werror",
)
KERNEL_ASFLAGS = (
    "-ffreestanding",
    "-fno-stack-protector",
    "-fno-pic",
    "-fno-pie",
    "-m64",
    "-mno-red-zone",
    "-Wall",
    "-Wextra",
    "-Werror",
)


class TrapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        ensure_build()
        cls.qemu = shutil.which("qemu-system-x86_64")
        cls.ovmf_code = find_firmware(OVMF_CODE_CANDIDATES)
        cls.ovmf_vars = find_firmware(OVMF_VARS_CANDIDATES)

    def build_test_kernel(self, directory: Path, vector: int) -> Path:
        main_object = directory / "trap_kernel.o"
        trigger_object = directory / "trap_trigger.o"
        kernel = directory / "kernel.elf"

        subprocess.run(
            [
                "g++",
                "-Iinclude",
                "-Ikernel",
                *KERNEL_CXXFLAGS,
                "-c",
                "tests/trap_kernel.cpp",
                "-o",
                str(main_object),
            ],
            cwd=ROOT,
            check=True,
        )
        subprocess.run(
            [
                "gcc",
                "-Iinclude",
                *KERNEL_ASFLAGS,
                f"-DSKERN_TRAP_TEST={vector}",
                "-c",
                "tests/trap_trigger.S",
                "-o",
                str(trigger_object),
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
                str(trigger_object),
                "-o",
                str(kernel),
            ],
            cwd=ROOT,
            check=True,
        )
        return kernel

    def run_trap(self, vector: int, inject_nmi: bool = False) -> str:
        if self.qemu is None or self.ovmf_code is None or self.ovmf_vars is None:
            self.skipTest("QEMU and OVMF are required for trap tests")

        with tempfile.TemporaryDirectory() as temporary_directory:
            directory = Path(temporary_directory)
            kernel = self.build_test_kernel(directory, vector)
            esp = directory / "esp"
            shutil.copytree(ROOT / "build" / "esp", esp)
            shutil.copyfile(kernel, esp / "skern" / "kernel.elf")
            writable_vars = directory / "OVMF_VARS.fd"
            shutil.copyfile(self.ovmf_vars, writable_vars)
            monitor = directory / "qmp.sock"

            command = [
                self.qemu,
                "-machine",
                "q35",
                "-m",
                "256M",
                "-display",
                "none",
                "-qmp",
                f"unix:{monitor},server=on,wait=off",
                "-serial",
                "stdio",
                "-no-reboot",
                "-drive",
                f"if=pflash,format=raw,readonly=on,file={self.ovmf_code}",
                "-drive",
                f"if=pflash,format=raw,file={writable_vars}",
                "-drive",
                f"format=raw,file=fat:rw:{esp}",
            ]
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                bufsize=0,
            )
            output = bytearray()
            selector = selectors.DefaultSelector()
            self.assertIsNotNone(process.stdout)
            selector.register(process.stdout, selectors.EVENT_READ)
            deadline = time.monotonic() + 15
            nmi_sent = False

            try:
                while time.monotonic() < deadline and re.search(
                    rb"R15=0x[0-9A-F]{16}", output
                ) is None:
                    if process.poll() is not None:
                        output.extend(process.stdout.read())
                        break
                    for key, _ in selector.select(timeout=0.25):
                        output.extend(os.read(key.fd, 4096))
                    if inject_nmi and not nmi_sent and b"TRAP TEST READY" in output:
                        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as connection:
                            connection.connect(str(monitor))
                            connection.recv(4096)
                            connection.sendall(b'{"execute":"qmp_capabilities"}\n')
                            connection.recv(4096)
                            connection.sendall(b'{"execute":"inject-nmi"}\n')
                            response = connection.recv(4096)
                            if b'"error"' in response:
                                self.fail(f"QMP failed to inject NMI: {response!r}")
                        nmi_sent = True
            finally:
                selector.close()
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=3)
                process.stdout.close()

            return output.decode(errors="replace")

    def test_invalid_opcode_preserves_all_general_purpose_registers(self) -> None:
        output = self.run_trap(6)
        self.assertIn("TRAP: Invalid Opcode", output)
        self.assertIn("VECTOR=0x0000000000000006", output)
        self.assertIn("ERROR=0x0000000000000000", output)

        registers = {
            "RAX": "0101010101010101",
            "RBX": "0202020202020202",
            "RCX": "0303030303030303",
            "RDX": "0404040404040404",
            "RSI": "0505050505050505",
            "RDI": "0606060606060606",
            "RBP": "0707070707070707",
            "R8": "0808080808080808",
            "R9": "0909090909090909",
            "R10": "1010101010101010",
            "R11": "1111111111111111",
            "R12": "1212121212121212",
            "R13": "1313131313131313",
            "R14": "1414141414141414",
            "R15": "1515151515151515",
        }
        for register, value in registers.items():
            with self.subTest(register=register):
                self.assertIn(f"{register}=0x{value}", output)

    def test_general_protection_fault_reports_cpu_error_code(self) -> None:
        output = self.run_trap(13)
        self.assertIn("TRAP: General Protection Fault", output)
        self.assertIn("VECTOR=0x000000000000000D", output)
        self.assertIn("ERROR=0x0000000000000038", output)

    def test_page_fault_reports_error_code_and_cr2(self) -> None:
        output = self.run_trap(14)
        self.assertIn("TRAP: Page Fault", output)
        self.assertIn("VECTOR=0x000000000000000E", output)
        self.assertIn("ERROR=0x0000000000000000", output)
        self.assertIn("CR2=0x0000400000000000", output)

    def test_nmi_reaches_the_dedicated_vector(self) -> None:
        output = self.run_trap(2, inject_nmi=True)
        self.assertIn("TRAP: Non-Maskable Interrupt", output)
        self.assertIn("VECTOR=0x0000000000000002", output)
        self.assertIn("ERROR=0x0000000000000000", output)

    def test_common_entry_preserves_frame_order_and_aligns_stack(self) -> None:
        result = subprocess.run(
            ["objdump", "-d", str(ROOT / "build" / "kernel" / "kernel.elf")],
            cwd=ROOT,
            check=True,
            capture_output=True,
            text=True,
        )
        match = re.search(
            r"<skern_trap_entry>:(.*?)(?=\n[0-9a-f]+ <|\Z)",
            result.stdout,
            re.DOTALL,
        )
        self.assertIsNotNone(match)
        entry = match.group(1)

        pushes = re.findall(r"push\s+%(\w+)", entry)
        pops = re.findall(r"pop\s+%(\w+)", entry)
        expected_pushes = [
            "rax",
            "rbx",
            "rcx",
            "rdx",
            "rsi",
            "rdi",
            "rbp",
            "r8",
            "r9",
            "r10",
            "r11",
            "r12",
            "r13",
            "r14",
            "r15",
        ]
        self.assertEqual(pushes, expected_pushes)
        self.assertEqual(pops, list(reversed(pushes)))
        alignment = entry.find("and    $0xfffffffffffffff0,%rsp")
        dispatch = entry.find("<skern_trap_dispatch>")
        self.assertGreaterEqual(alignment, 0)
        self.assertGreater(dispatch, alignment)


if __name__ == "__main__":
    unittest.main()
