OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS {
    . = 1M;

    .text : {
        KEEP(*(.text._start))
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
        *(.requests)
    }

    .data : {
        *(.data*)
    }

    .bss : {
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