# skern

skern is a hobby x86-64 operating system with a custom UEFI bootloader, kernel, C library, and userspace utilities. The project is in early development.

## Project layout

```text
skern/
├── bootloader/ # UEFI bootloader (C, x86-64 Assembly)
├── include/    # Shared bootloader/kernel ABI headers (C)
├── kernel/     # Freestanding kernel (C++, x86-64 Assembly)
├── libc/       # C standard library (C, C++, x86-64 Assembly)
├── utils/      # Userspace utilities (C, C++)
└── tests/      # Host and integration tests (Python)
```

## Roadmap for 1.0

Incomplete phases use checklists to describe planned work. Completed phases describe the resulting implementation and the contracts later phases may rely on.

### Phase 1: Kernel entry and boot information (complete)

The GNU-EFI bootloader validates and loads a static x86-64 ELF kernel from the EFI system partition. Each loadable segment is allocated at its ELF physical address, copied into memory, and zero-filled through `p_memsz`. The linked kernel uses matching physical and virtual load addresses. Its page-aligned physical extent is exposed through `__kernel_start` and `__kernel_end`.

The bootloader constructs version 2 of the shared `BootInfo` ABI. It provides the final UEFI memory map and descriptor metadata, GOP framebuffer address, byte size and mode information, and the bootloader-allocated bootstrap stack. It retries `ExitBootServices()` with a refreshed memory map when the firmware invalidates the map key.

Kernel entry follows the System V AMD64 ABI. The assembly handoff installs the aligned bootstrap stack, supplies a synthetic return address, clears the previous frame pointer, passes `BootInfo` in `RDI`, and jumps to the ELF entry point. The kernel rejects a null pointer or an unexpected magic value, version, or structure size before using the handoff data.

At entry, the kernel relies on the firmware's active identity mappings for the kernel image, `BootInfo`, memory-map buffer, bootstrap stack, and framebuffer. Their addresses and extents are available from linker symbols or `BootInfo`. Phase 4 must reproduce these mappings in kernel-owned page tables before switching `CR3`.

Early diagnostics use COM1 after a loopback probe. `panic()` disables interrupts, writes its message to serial and, when initialized, the framebuffer console, then halts. Host tests cover ELF validation and the C/C++ layout of the shared ABI; QEMU tests verify the kernel ELF assumptions and boot with 64, 128, 256, and 512 MiB of memory.

### Phase 2: Kernel console and diagnostics (complete)

The kernel validates the framebuffer supplied through `BootInfo`, including its address, dimensions, scanline stride, byte extent, alignment, and pixel format. The framebuffer layer provides bounded pixel writes and row operations while translating logical `0x00RRGGBB` colors into the UEFI RGB or BGR memory layout. It uses the firmware's identity mapping until Phase 4 installs kernel-owned page tables.

The text console renders a single embedded 16-by-32 grayscale bitmap font. Coverage values blend foreground and background colors to produce antialiased glyphs. The console tracks its cursor in character cells, handles wrapping, newline, carriage return, tab, and backspace, and scrolls by moving rendered pixel rows upward. It intentionally keeps only the visible rendered pixels and does not provide scrollback history.

Boot messages and panic diagnostics are written to both COM1 and the framebuffer, with serial attempted first as the debugging fallback. Console output before successful initialization is a safe no-op, and the entire path operates without a heap or scheduler. Host tests exercise validation failures, pixel formats and bounds, row operations, glyph placement, wrapping, control characters, pre-initialization output, and scrolling. QEMU smoke tests confirm that framebuffer initialization succeeds while booting with 64, 128, 256, and 512 MiB of memory.

### Phase 3: CPU foundation

- [x] Install the GDT.
- [x] Create a TSS.
- [x] Install the IDT.
- [x] Establish interrupt-entry assembly stubs.
- [x] Handle CPU exceptions with useful register dumps.
- [x] Use an interrupt-safe kernel stack for critical exceptions.
- [x] Trigger representative exceptions in QEMU and validate their dumps.
- [x] Verify that assembly stubs preserve CPU state and stack alignment.

**Checkpoint A:** The kernel boots, prints logs, and handles exceptions.

### Phase 4: Memory management

- [ ] Parse and normalize the UEFI memory map.
- [ ] Create a physical page-frame allocator.
- [ ] Reserve the kernel, `BootInfo`, framebuffer, page tables, and boot data.
- [ ] Build kernel-owned x86-64 page tables.
- [ ] Preserve every required mapping during the page-table transition.
- [ ] Switch `CR3` and verify that serial and framebuffer output still work.
- [ ] Reclaim bootloader memory only after all references to it are gone.
- [ ] Implement virtual address mapping and unmapping.
- [ ] Add a kernel heap.
- [ ] Test memory-map normalization, allocation, exhaustion, mapping permissions, reserved ranges, and TLB invalidation.

### Phase 5: Interrupts, minimal ACPI, and time

- [ ] Locate and validate the ACPI RSDP and required root tables.
- [ ] Parse the MADT for local APIC, I/O APIC, interrupt override, and CPU data.
- [ ] Disable the legacy PIC.
- [ ] Initialize the bootstrap processor's local APIC and the I/O APIC.
- [ ] Configure a timer.
- [ ] Dispatch and acknowledge hardware interrupts safely.
- [ ] Track monotonic kernel time.
- [ ] Test ACPI parsing against captured tables and malformed inputs.
- [ ] Verify interrupt masking, acknowledgement, timer rate, and monotonic time.

**Checkpoint B:** The kernel owns memory management and receives timer interrupts.

### Phase 6: Hardware discovery and essential drivers

- [ ] Add a device and driver abstraction only as real drivers require it.
- [ ] Implement a PS/2 keyboard driver.
- [ ] Enumerate PCI devices before storage hardware requires it.
- [ ] Extend ACPI support only for concrete hardware needs.
- [ ] Test scan-code decoding independently of the keyboard hardware driver.
- [ ] Verify keyboard input under QEMU without lost or repeated events.

### Phase 7: Kernel execution

- [ ] Define kernel tasks and their saved CPU contexts.
- [ ] Implement kernel stack creation and context switching.
- [ ] Add a single-core preemptive scheduler.
- [ ] Add spinlocks, wait queues, and event notification primitives.
- [ ] Define the rules for interrupt, scheduler, and lock interactions.
- [ ] Give kernel stacks guard pages and preserve their required alignment.
- [ ] Add scheduler invariants in debug builds.
- [ ] Stress preemption, blocking, waking, termination, and synchronization.

**Checkpoint C:** Multiple kernel threads run under preemptive scheduling.

### Phase 8: Processes and userspace transitions

- [ ] Define processes separately from schedulable threads.
- [ ] Create per-process address spaces and anonymous userspace mappings.
- [ ] Give every userspace thread a kernel stack and update `TSS.RSP0` on switch.
- [ ] Define the complete saved user context and address-space switch procedure.
- [ ] Implement interrupt and exception entry when the interrupted code is in ring 3.
- [ ] Implement the syscall ABI and `SYSCALL` entry path.
- [ ] Validate all user pointers, lengths, handles, and syscall arguments.
- [ ] Implement a safe return path to userspace without trusting unsafe return state.
- [ ] Load statically linked userspace ELF executables from an initramfs.
- [ ] Implement process creation, termination, and waiting.
- [ ] Test ELF parsing and syscall validation with malformed inputs.
- [ ] Verify process isolation and contain userspace faults within their process.

**Checkpoint D:** The first isolated, statically linked userspace process executes from the initramfs.

### Phase 9: Descriptors and interprocess communication

- [ ] Define a common kernel I/O object interface.
- [ ] Give each process a descriptor table.
- [ ] Implement `read`, `write`, and `close` over descriptors.
- [ ] Implement pipes using the same descriptor model.
- [ ] Integrate pipe operations with scheduler blocking and wakeups.
- [ ] Define partial-operation, end-of-file, broken-pipe, and process-exit behavior.
- [ ] Add polling or a similar mechanism when userspace requires it.
- [ ] Defer message ports and shared-memory objects until a real use case needs them.
- [ ] Stress pipe buffering, closure, multiple readers and writers, and peer exits.

**Checkpoint E:** Multiple isolated userspace processes communicate through pipes.

### Phase 10: Userspace foundation

- [ ] Implement syscall wrappers and the required statically linked libc subset.
- [ ] Create `/sbin/init` and load it from the initramfs.
- [ ] Add basic userspace service startup and supervision.
- [ ] Add utilities such as `echo`, `cat`, `ls`, `mkdir`, and `ps`.
- [ ] Add an interactive shell with pipelines, process waiting, and exit statuses.
- [ ] Provide initial `/bin`, `/dev`, `/etc`, and `/tmp` contents.
- [ ] Add QEMU tests that boot to init and exercise utilities and pipelines.
- [ ] Run several isolated userspace processes concurrently for extended periods.

**Checkpoint F:** An interactive shell runs userspace utilities from the initramfs.

### Phase 11: I/O, filesystems, and persistent storage

- [ ] Extend the descriptor model with filesystem-backed objects.
- [ ] Build a VFS and path-resolution layer.
- [ ] Expose devices through a consistent userspace interface.
- [ ] Add a block-device abstraction.
- [ ] Implement one QEMU-friendly storage driver.
- [ ] Implement FAT32 as the first persistent filesystem.
- [ ] Mount persistent storage without making it a prerequisite for early userspace.
- [ ] Load programs and data from storage.
- [ ] Preserve files across clean reboots.
- [ ] Test path resolution, malformed media, full disks, and I/O failures.
- [ ] Add QEMU tests that create, modify, reboot, and reload files.

FAT32 is the initial choice for interoperability with host tools and easy disk-image inspection. A custom filesystem remains an optional project if filesystem design later becomes a learning objective.

**Checkpoint G:** Files created by userspace persist across reboots.

### Phase 12: Symmetric multiprocessing

- [ ] Discover and start application processors.
- [ ] Create per-CPU data, stacks, TSS instances, and scheduler state.
- [ ] Make memory management and kernel synchronization SMP-safe.
- [ ] Schedule work across multiple CPU cores.
- [ ] Handle inter-processor interrupts and TLB shootdowns.
- [ ] Keep single-core operation as a supported fallback.
- [ ] Stress concurrent allocation, process creation, IPC, and filesystem access.
- [ ] Run the integration suite with one, two, and four virtual CPUs.
- [ ] Confirm reliability under randomized preemption timing.

**Checkpoint H:** Checkpoints A through G work reliably with 1 to 4 CPUs.

## Optional extensions

- [ ] Add shared-memory IPC when a real use case needs it.
- [ ] Add networking and a socket API.
- [ ] Add USB and USB HID support.
- [ ] Add audio support.
- [ ] Add more storage devices and filesystems.
- [ ] Improve permissions, security, and process isolation.
- [ ] Expose the framebuffer to userspace for graphical demonstrations.
- [ ] Reconsider a graphics stack only if it becomes a project goal.

## First major release

The release is complete when skern can reliably boot on 1 to 4 x86-64 CPUs, mount persistent storage, start `/sbin/init`, reach an interactive shell, run multiple isolated processes with preemptive scheduling, connect them through pipelines, preserve files across reboots, and pass the automated host and QEMU integration test suite.
