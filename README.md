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

The GNU-EFI bootloader validates and loads a static x86-64 ELF kernel from the EFI system partition. Each loadable segment is allocated at its low ELF physical address, copied into memory, zero-filled through `p_memsz`, and mapped at its higher-half virtual address with permissions derived from the ELF flags. The kernel is linked at `0xFFFFFFFF80000000`, loaded from physical address `0x200000`, and exposes page-aligned virtual and physical bounds through linker symbols.

The bootloader constructs version 2 of the shared `BootInfo` ABI. It provides the final UEFI memory map and descriptor metadata, GOP framebuffer address, byte size and mode information, and the bootloader-allocated bootstrap stack. It retries `ExitBootServices()` with a refreshed memory map when the firmware invalidates the map key.

Kernel entry follows the System V AMD64 ABI. The assembly handoff installs the aligned bootstrap stack, supplies a synthetic return address, clears the previous frame pointer, passes `BootInfo` in `RDI`, and jumps to the ELF entry point. The kernel rejects a null pointer or an unexpected magic value, version, or structure size before using the handoff data.

Before entry, the bootloader constructs the initial x86-64 page tables, enables execute-disable support, loads their physical root into `CR3`, installs the bootstrap stack through its HHDM alias, and jumps to the higher-half ELF entry. Temporary identity mappings keep early physical-address users reachable until the kernel adopts the tables and removes the lower half during memory initialization.

Early diagnostics use COM1 after a loopback probe. `panic()` disables interrupts, writes its message to serial and, when initialized, the framebuffer console, then halts. Host tests cover ELF validation and the C/C++ layout of the shared ABI; QEMU tests verify the kernel ELF assumptions and boot with 64, 128, 256, and 512 MiB of memory.

### Phase 2: Kernel console and diagnostics (complete)

The kernel validates the framebuffer supplied through `BootInfo`, including its address, dimensions, scanline stride, byte extent, alignment, and pixel format. The framebuffer layer accesses it through the HHDM, provides bounded pixel writes and row operations, and translates logical `0x00RRGGBB` colors into the UEFI RGB or BGR memory layout.

The text console renders a single embedded 16-by-32 grayscale bitmap font. Coverage values blend foreground and background colors to produce antialiased glyphs. The console tracks its cursor in character cells, handles wrapping, newline, carriage return, tab, and backspace, and scrolls by moving rendered pixel rows upward. It intentionally keeps only the visible rendered pixels and does not provide scrollback history.

Boot messages and panic diagnostics are written to both COM1 and the framebuffer, with serial attempted first as the debugging fallback. Console output before successful initialization is a safe no-op, and the entire path operates without a heap or scheduler. Host tests exercise validation failures, pixel formats and bounds, row operations, glyph placement, wrapping, control characters, pre-initialization output, and scrolling. QEMU smoke tests confirm that framebuffer initialization succeeds while booting with 64, 128, 256, and 512 MiB of memory.

### Phase 3: CPU foundation (complete)

The kernel installs its own GDT with ring 0 and ring 3 code and data segments, plus a 64-bit TSS descriptor. The TSS provides separate 16 KiB interrupt stacks for double fault, non-maskable interrupt, and machine check. These critical exceptions use IST1, IST2, and IST3 respectively, so their handlers do not depend on the interrupted stack remaining usable.

The IDT contains an interrupt gate for every vector from 0 through 255. Per-vector assembly stubs normalize the hardware frame by supplying a synthetic error code where the CPU does not provide one, then record the vector number and enter a shared trap path. The common entry saves every general-purpose register, clears the direction flag, aligns the stack for the System V AMD64 ABI, and presents a uniform `TrapFrame` to C++ code.

CPU exceptions currently terminate the kernel after reporting the exception name, vector, error code, instruction pointer, code segment, flags, and general-purpose registers to both serial and the framebuffer console. Page faults additionally report `CR2`. QEMU integration tests trigger invalid opcode, general-protection fault, page fault, and non-maskable interrupt, validating register preservation, CPU-provided error codes, `CR2`, and critical-stack delivery. Static inspection tests verify the common entry's register order, restoration order, and call-site stack alignment.

### Phase 4: Memory management (complete)

The bootloader constructs 4-level x86-64 page tables before `ExitBootServices()`. Managed RAM and the framebuffer receive temporary identity mappings and permanent HHDM aliases beginning at `0xFFFF800000000000`; ELF segments are mapped at their linked higher-half addresses with writable and execute-disable permissions derived from their program headers. After all page-table allocations, the bootloader obtains the final UEFI memory map and key, exits boot services, enables `EFER.NXE`, loads the new root into `CR3`, and transfers control to the kernel.

The kernel validates the final UEFI descriptor array, converts firmware types into kernel memory types, sorts the regions by physical address, rejects overlaps, and merges adjacent regions with matching type and attributes. The physical memory manager tracks managed frames with separate allocation and protection bitmaps stored in usable RAM. The kernel image, `BootInfo`, normalized memory-map storage, bootstrap stack, PMM metadata, and every active page-table frame are protected from allocation; framebuffer and other MMIO regions are never added to the managed pool.

The virtual memory manager adopts the active root from `CR3` and accesses every page-table frame through the HHDM. It can map and unmap 4 KiB pages, applies writable and execute-disable permissions, and invalidates the affected TLB entry after unmapping. Once all physical pointers and the active stack use higher-half aliases, the kernel removes the lower PML4 entries, releases the page-table frames used only by identity mappings, and reloads `CR3`. Serial and framebuffer output remain operational across the transition.

After installing the kernel GDT, TSS, and IDT, the PMM releases unprotected UEFI loader and boot-services frames. Pages that still back retained boot information, memory-map data, the bootstrap stack, PMM metadata, or active page tables remain protected.

The kernel heap reserves the 512 MiB virtual range from `0xFFFFFFFFC0000000` through `0xFFFFFFFFDFFFFFFF`. It grows by mapping PMM frames at consecutive virtual addresses as writable, non-executable pages. A first-fit allocator stores doubly linked block headers in the heap, returns 16-byte-aligned payloads, splits oversized free blocks, coalesces adjacent free blocks, reuses released storage, and panics when physical memory or the reserved virtual range is exhausted. It does not return mapped pages to the PMM yet.

Host tests exercise memory-map validation, sorting, merging, overlap rejection, and type conversion. A dedicated QEMU kernel tests PMM allocation, reuse, protection and exhaustion; VMM permissions, remapping, unmapping and TLB invalidation; and heap alignment, separation, growth, reuse and coalescing. Boot tests continue to verify the higher-half transition with 64, 128, 256, and 512 MiB of RAM.

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
