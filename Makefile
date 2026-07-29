# HisokaOS build — Apple clang cross-compiles to i686-elf; i686-elf-ld links the
# ELF kernel; QEMU boots it via Multiboot (no GRUB/ISO needed for `make run`).

CC      := clang
LD      := i686-elf-ld
CFLAGS  := -target i686-elf -ffreestanding -nostdlib -fno-stack-protector -fno-pic \
           -mno-sse -mno-mmx -mno-80387 -Wall -Wextra -O2 -std=gnu11 -Iinclude
ASFLAGS := -target i686-elf
LDFLAGS := -T kernel/linker.ld -nostdlib

CSRC := $(wildcard kernel/*.c cpu/*.c drivers/*.c mm/*.c lib/*.c security/*.c fs/*.c apps/*.c net/*.c)
SSRC := boot/boot.s $(wildcard cpu/*.s)
OBJ  := $(CSRC:.c=.o) $(SSRC:.s=.o)

KERNEL := hisoka.elf

all: $(KERNEL)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)
	@echo "built $(KERNEL)"

# boot it in QEMU (serial mirrored to your terminal). The rtl8139 NIC on QEMU's
# user-mode network is what the PCI scan / `net` command discovers.
run: $(KERNEL)
	@[ -f disk.img ] || qemu-img create -f raw disk.img 32M
	qemu-system-i386 -kernel $(KERNEL) -serial stdio -m 128M \
		-drive file=disk.img,format=raw,if=ide \
		-netdev user,id=n0 -device rtl8139,netdev=n0

# verify it's a valid Multiboot kernel without booting
check: $(KERNEL)
	@grub-file --is-x86-multiboot $(KERNEL) && echo "valid multiboot kernel" || echo "NOT multiboot"

clean:
	rm -f $(OBJ) $(KERNEL)

.PHONY: all run check clean
