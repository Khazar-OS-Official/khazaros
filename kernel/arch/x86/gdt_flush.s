; GDT Flush - LGDT instruction ile GDT'yi yükle
; NASM syntax

global gdt_flush        ; C'den çağrılabilir
global tss_flush
extern gdt_pointer      ; C'deki GDT pointer

gdt_flush:
    ; Stack'ten GDT pointer adresini al
    mov eax, [esp + 4]
    
    ; LGDT instruction - GDT'yi GDTR register'ına yükle
    lgdt [eax]
    
    ; Segment register'larını güncelle
    ; Data segment'leri hemen güncellenebilir
    mov ax, 0x10        ; Kernel Data Segment (GDT entry 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Code segment (CS) sadece far jump ile güncellenebilir
    ; 0x08: Kernel Code Segment (GDT entry 1)
    jmp 0x08:.flush
    
.flush:
    ret

tss_flush:
    mov ax, 0x2B      ; TSS descriptor offset (0x28 | 3 for RPL 3)
    ltr ax            ; Load Task Register
    ret
