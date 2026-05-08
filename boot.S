.section ".text.boot"

.global _start

_start:
    mrs     x0, mpidr_el1
    and     x0, x0, #3
    cbz     x0, _core0

_park:
    wfe
    b       _park

_core0:
    ldr     x0, =_start
    mov     sp, x0

    ldr     x0, =__bss_start
    ldr     x1, =__bss_end

_bss_loop:
    cmp     x0, x1
    b.ge    _bss_done
    str     xzr, [x0], #8
    b       _bss_loop

_bss_done:
    bl      kernel_main
    b       _park
