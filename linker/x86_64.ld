OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_VIRT_OFFSET = 0xffffffff80000000;
KERNEL_PHYS_BASE  = 0x100000;

SECTIONS {
    . = KERNEL_VIRT_OFFSET + KERNEL_PHYS_BASE;

    .requests : AT(ADDR(.requests) - KERNEL_VIRT_OFFSET) {
        KEEP(*(.requests.start))
        KEEP(*(.requests))
        KEEP(*(.requests.end))
    }

    .text : AT(ADDR(.text) - KERNEL_VIRT_OFFSET) {
        KEEP(*(.text._start))
        *(.text*)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_OFFSET) {
        *(.rodata*)
    }

    .data : AT(ADDR(.data) - KERNEL_VIRT_OFFSET) {
        *(.data*)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VIRT_OFFSET) {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.debug*)
        *(.gnu*)
    }
}
