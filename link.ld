ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text : {
        KEEP(*(.text.boot))
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    . = ALIGN(16);
    __bss_start = .;
    .bss (NOLOAD) : {
        *(.bss)
        *(COMMON)
    }
    . = ALIGN(16);
    __bss_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
