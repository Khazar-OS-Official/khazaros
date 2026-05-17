; 64-bit GDT flush
global gdt_flush64
extern gdt64_ptr

gdt_flush64:
    lgdt [gdt64_ptr]
    mov ax, 0x10  ; Data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far jump to reload CS
    push 0x08  ; Code segment
    push .reload_cs
    retfq
.reload_cs:
    ret
