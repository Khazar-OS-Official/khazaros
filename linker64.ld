/* Khazar OS 64-bit Linker Script */

ENTRY(_start)

KERNEL_VIRTUAL_BASE = 0xFFFFFFFF80000000;  /* Higher half: -2GB */

SECTIONS
{
    /* Bootloader runs at low physical address (1MB) */
    .boot 0x100000 : AT(0x100000)
    {
        boot/boot64.o(.multiboot)
        boot/boot64.o(.text)
        boot/gdt_flush64.o(.text)
        boot/interrupt64.o(.text)
    }
    
    /* Kernel at higher half (starts after bootloader) */
    . = KERNEL_VIRTUAL_BASE + 0x110000;  /* After bootloader */

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE)
    {
        *(.text)
        EXCLUDE_FILE (boot/boot64.o boot/gdt_flush64.o boot/interrupt64.o) *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE)
    {
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE)
    {
        *(.data)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE)
    {
        bss_start = .;
        *(COMMON)
        *(.bss)
        . = ALIGN(4K);
        bss_end = .;
    }

    . = ALIGN(4K);
    _kernel_end = .;
}
