/* Khazar OS Linker Script
 * Higher Half Kernel (0xC0000000) yapılandırması
 *
 * FİZİKSEL: 0x00100000 (1MB) adresine yüklenir (LMA)
 * SANAL:    0xC0100000 (3GB+1MB) adresinde çalışır (VMA)
 *
 * KRİTİK NOT:
 *   kernel stack boot.asm'da .data section içinde tanımlanmıştır.
 *   Böylece BSS temizleme (rep stosb) stack'i sıfırlamaz.
 */

ENTRY(_start)

SECTIONS
{
    . = 0xC0100000;

    /* .multiboot: GRUB'un aradığı magic - ilk 8KB içinde olmalı */
    .multiboot BLOCK(4K) : AT(ADDR(.multiboot) - 0xC0000000)
    {
        *(.multiboot)
    }

    /* .text: Kod */
    .text BLOCK(4K) : AT(ADDR(.text) - 0xC0000000)
    {
        *(.text)
    }

    /* .rodata: Read-only data */
    .rodata BLOCK(4K) : AT(ADDR(.rodata) - 0xC0000000)
    {
        *(.rodata)
    }

    /* .data: Initialized data
     * boot_page_directory, boot_page_tables, kernel_stack buradadır */
    .data BLOCK(4K) : AT(ADDR(.data) - 0xC0000000)
    {
        *(.data)
    }

    /* .bss: Uninitialized data (C global değişkenleri)
     * Stack .data'da olduğundan buranın temizlenmesi güvenli */
    .bss BLOCK(4K) : AT(ADDR(.bss) - 0xC0000000)
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
