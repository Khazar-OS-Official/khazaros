; Khazar OS 64-bit Bootloader
; Multiboot 2 standardıyla uyumlu (64-bit üçün)

; Note: default rel only works in 64-bit mode, so we don't use it here

; Multiboot 2 constants
MB2_MAGIC equ 0xE85250D6
MB2_ARCH equ 0  ; i386/x86_64
MB2_HEADER_LENGTH equ (mb2_header_end - mb2_header_start)
MB2_CHECKSUM equ -(MB2_MAGIC + MB2_ARCH + MB2_HEADER_LENGTH)

; Page table constants
PML4_TABLE equ 0x1000
PDP_TABLE equ 0x2000
PD_TABLE equ 0x3000
PT_TABLE equ 0x4000

; Flags
PAGE_PRESENT equ 1 << 0
PAGE_WRITABLE equ 1 << 1
PAGE_HUGE equ 1 << 7

SECTION .multiboot
ALIGN 8
mb2_header_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_HEADER_LENGTH
    dd MB2_CHECKSUM
    
    ; End tag
    dw 0  ; type
    dw 0  ; flags
    dd 8  ; size
mb2_header_end:

SECTION .bss
ALIGN 4096
stack_bottom:
    resb 16384  ; 16 KB stack
stack_top:

SECTION .text
BITS 32
global _start
_start:
    ; Multiboot info saxla (stack-də saxla, çünki EDI paging üçün lazımdır)
    push eax  ; Magic
    push ebx  ; MBI pointer

    ; Clear page tables first
    mov edi, PML4_TABLE
    mov ecx, 4096  ; 4KB = 512 entries * 8 bytes
    xor eax, eax
    rep stosb
    
    ; Paging setup (4-level paging for 64-bit)
    ; PML4 - Entry 0 points to PDP
    mov edi, PML4_TABLE
    mov eax, PDP_TABLE | PAGE_PRESENT | PAGE_WRITABLE
    mov [edi], eax
    
    ; PDP - Entry 0 points to PD
    mov edi, PDP_TABLE
    mov eax, PD_TABLE | PAGE_PRESENT | PAGE_WRITABLE
    mov [edi], eax
    
    ; PD (2MB pages) - Identity map first 1GB
    mov edi, PD_TABLE
    mov eax, 0 | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE
    mov ecx, 512  ; Map 512 * 2MB = 1GB
set_pd_loop:
    mov [edi], eax
    add eax, 0x200000  ; 2MB
    add edi, 8
    dec ecx
    jnz set_pd_loop
    
    ; Map higher half (0xFFFFFFFF80000000 -> 0x0) - PML4 entry 511
    ; PML4[511] -> PDP_HIGH (points to same PD_TABLE for identity mapping)
    mov edi, 0x5000  ; Second PDP at 0x5000
    mov eax, PD_TABLE | PAGE_PRESENT | PAGE_WRITABLE
    mov [edi], eax  ; PDP_HIGH[0] -> same PD_TABLE (identity map)
    
    ; PML4[511] -> PDP_HIGH (maps 0xFFFFFFFF80000000-0xFFFFFFFFC0000000 -> 0x0-0x40000000)
    mov edi, PML4_TABLE
    mov eax, 0x5000 | PAGE_PRESENT | PAGE_WRITABLE
    mov [edi + 511 * 8], eax  ; Entry 511 (top 512GB of 48-bit space)

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5  ; PAE bit
    mov cr4, eax

    ; Set PML4
    mov eax, PML4_TABLE
    mov cr3, eax

    ; Enable long mode (EFER)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8  ; LME bit
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31  ; PG bit
    mov cr0, eax

    ; Load 64-bit GDT
    lgdt [gdt64_ptr]
    
    ; Jump to 64-bit code segment
    jmp 0x08:long_mode_start

BITS 64
long_mode_start:
    ; Setup segments
    mov ax, 0x10  ; Data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Setup stack (stack_top is in bootloader BSS, identity mapped at low address)
    ; stack_top is around 0x100000 + bootloader_size, which is identity mapped
    mov rsp, stack_top

    ; Clear RFLAGS
    push 0
    popf

    ; TEST: VGA buffer-a yaz (0xB8000 identity mapped)
    mov rdi, 0xB8000
    mov word [rdi], 0x0F41  ; White 'A' on black
    mov word [rdi + 2], 0x0F42  ; White 'B' on black
    mov word [rdi + 4], 0x0F43  ; White 'C' on black
    mov word [rdi + 6], 0x0F20  ; Space
    mov word [rdi + 8], 0x0F4F  ; 'O'
    mov word [rdi + 10], 0x0F4B  ; 'K'

    ; Multiboot info (x86_64 calling convention: RDI = first arg, RSI = second arg)
    ; Stack-dən pop et: LIFO - son push edilən ilk çıxır
    pop rsi  ; MBI pointer (second arg) - son push edilən (EBX)
    pop rdi  ; Magic (first arg) - ilk push edilən (EAX)

    ; BSS clear - kernel BSS (virtual addresses, already mapped)
    ; Temporarily skip BSS clear to test if it's causing the crash
    ; extern bss_start
    ; extern bss_end
    ; mov rax, bss_start  ; Virtual address (0xFFFFFFFF80110000+)
    ; mov rcx, bss_end
    ; sub rcx, rax
    ; test rcx, rcx
    ; jz skip_bss_clear
    ; mov rdi, rax
    ; xor rax, rax
    ; cld  ; Clear direction flag (forward)
    ; rep stosb
; skip_bss_clear:

    ; Call kernel_main
    extern kernel_main
    call kernel_main

.halt:
    cli
    hlt
    jmp .halt

; 64-bit GDT (text section-da saxla relocation üçün)
SECTION .text
ALIGN 8
gdt64:
    dq 0  ; Null descriptor
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)  ; Code segment (64-bit)
    dq (1 << 44) | (1 << 47)  ; Data segment
gdt64_end:

global gdt64_ptr
gdt64_ptr:
    dw gdt64_end - gdt64 - 1  ; Limit (16-bit)
    dd gdt64                   ; Base address (32-bit for 32-bit mode lgdt)
