# skern

skern is a hobby x86-64 OS with a custom UEFI bootloader, kernel, C library, and userspace utilities. The project is in early development.

## Project layout

```text
skern/
├── boot/       # UEFI bootloader (C, x86-64 Assembly)
├── kernel/     # Freestanding kernel (C++, x86-64 Assembly)
├── libc/       # C standard library (C, C++, x86-64 Assembly)
├── utils/      # Userspace utilities (C, C++)
└── tests/      # Host and integration tests (Python)
```
