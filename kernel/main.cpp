extern "C" void kernel_main() {
    const char *message = "Hello from skern!\n";

    for (const char *p = message; *p != '\0'; ++p) {
        asm volatile("outb %0, $0xE9" : : "a"(*p));
    }

    while (true) {
        asm volatile("cli; hlt");
    }
}
