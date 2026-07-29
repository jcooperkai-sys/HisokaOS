# HisokaOS

A 32-bit x86 operating system written from scratch in C and assembly. It boots on its own kernel —
there is no Linux or BSD underneath — and comes up into an interactive text-mode environment with a
shell, a windowing UI layer, and a set of bundled applications.

![HisokaOS booting](docs/boot.png)
![First-run setup](docs/setup.png)

*Captured from QEMU: the kernel's boot screen, and the first-run setup wizard.*

## Overview

Roughly 10,000 lines of C and assembly across 115 source files. The kernel handles memory,
interrupts, scheduling and device I/O directly against the hardware; everything above it — the
filesystem, the shell, the UI, the apps — is built on those primitives.

## Kernel

| Subsystem | Location | Notes |
|---|---|---|
| Multiboot entry, linker script | `boot/`, `kernel/linker.ld` | boots via QEMU `-kernel` |
| GDT with ring0/ring3 descriptors | `cpu/gdt.c` | |
| IDT, CPU exception handlers (0–31) | `cpu/idt.c`, `cpu/isr.c` | faults are trapped, not fatal |
| IRQs via remapped 8259 PIC | `drivers/pic.c` | |
| Preemptive task switching | `cpu/task.c`, `cpu/switch.s` | context switch in assembly |
| Physical memory manager | `mm/pmm.c` | frame bitmap over Multiboot memory map |
| Paging | `mm/paging.c` | |
| Kernel heap | `mm/heap.c` | first-fit `kmalloc`/`kfree` |
| PCI bus enumeration | `cpu/pci.c` | |

## Drivers

VGA text console with scrolling and colour, graphics mode (`gfx.c`), PS/2 keyboard with shift
handling and a ring buffer, PS/2 mouse, PIT timer at 100 Hz, RTC, PC speaker, serial output on COM1,
ATA disk, and an RTL8139 network interface that reads its own MAC off the card.

## Filesystem

A RAM filesystem (`fs/ramfs.c`), a `/proc`-style synthetic filesystem exposing kernel state
(`fs/procfs.c`), and persistence to disk (`fs/persist.c`).

## Userland

27 bundled applications in `apps/`, including a text editor, file explorer, image viewer, drawing
program, calculator, clock, media player, settings panel, backup tool, and games — Tetris, Snake,
2048 and tic-tac-toe. A shared UI layer (`apps/ui.c`) provides the windowing and widget primitives
they draw with.

## Building

Requires an `i686-elf` cross-toolchain and QEMU:

```bash
brew install i686-elf-binutils qemu
make            # produces hisoka.elf
make run        # boots in QEMU, serial mirrored to the terminal
```

`dev-vm.sh` rebuilds and relaunches the VM over VNC for iterative work.

## Known limitations

These are real and worth knowing before reading the code:

- `mm/pmm.c` assumes contiguous RAM above 1 MiB rather than walking the full Multiboot memory map.
- `mm/heap.c` coalesces freed blocks forward only, so long-running allocation churn can fragment.
- `lib/printf.c` implements `%c %s %d %u %x %p` without field width or zero-padding.
- `drivers/keyboard.c` supports US layout and shift; no caps lock or extended keys.
- Applications run in ring 0. Ring 3 descriptors exist in the GDT, but userspace isolation needs a
  TSS and syscall boundary that are not wired up yet.

## Licence

MIT — see [LICENSE](LICENSE).
