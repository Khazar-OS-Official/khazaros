; Interrupt Service Routines - Assembly Stubs
; NASM syntax

; C fonksiyonları
extern isr_handler
extern irq_handler

; Common ISR stub - tüm ISR'lar buraya atlar
global isr_common_stub
isr_common_stub:
    ; Register'ları kaydet
    pusha                    ; EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI push
    
    mov ax, ds               ; Data segment'i kaydet
    push eax
    
    mov ax, 0x10             ; Kernel data segment yükle
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; C handler'ı çağır
    push esp                 ; Stack pointer (registers struct pointer)
    call isr_handler
    mov esp, eax             ; Switch stack if scheduler returned a new one
    
    ; Register'ları restore et
    pop eax                  ; Data segment'i restore
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                     ; Register'ları restore
    add esp, 8               ; Error code ve interrupt number'ı temizle
    iret                     ; Interrupt'tan dön

; Common IRQ stub
global irq_common_stub
irq_common_stub:
    pusha
    
    mov ax, ds
    push eax
    
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp
    call irq_handler
    mov esp, eax
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa
    add esp, 8
    iret

; ISR Macro - error code olmayan exception'lar için
%macro ISR_NOERRCODE 1
    global isr%1
    isr%1:
        cli                  ; Interrupt'ları devre dışı bırak
        push 0               ; Dummy error code
        push %1              ; Interrupt number
        jmp isr_common_stub
%endmacro

; ISR Macro - error code olan exception'lar için
%macro ISR_ERRCODE 1
    global isr%1
    isr%1:
        cli
        ; CPU zaten error code push etti
        push %1              ; Interrupt number
        jmp isr_common_stub
%endmacro

; IRQ Macro
%macro IRQ 2
    global irq%1
    irq%1:
        cli
        push 0               ; Dummy error code
        push %2              ; Interrupt number (32 + IRQ number)
        jmp irq_common_stub
%endmacro

; Exception ISRs (0-31)
ISR_NOERRCODE 0     ; Division by zero
ISR_NOERRCODE 1     ; Debug
ISR_NOERRCODE 2     ; Non-maskable interrupt
ISR_NOERRCODE 3     ; Breakpoint
ISR_NOERRCODE 4     ; Overflow
ISR_NOERRCODE 5     ; Bound range exceeded
ISR_NOERRCODE 6     ; Invalid opcode
ISR_NOERRCODE 7     ; Device not available
ISR_ERRCODE   8     ; Double fault (error code)
ISR_NOERRCODE 9     ; Coprocessor segment overrun
ISR_ERRCODE   10    ; Invalid TSS (error code)
ISR_ERRCODE   11    ; Segment not present (error code)
ISR_ERRCODE   12    ; Stack-segment fault (error code)
ISR_ERRCODE   13    ; General protection fault (error code)
ISR_ERRCODE   14    ; Page fault (error code)
ISR_NOERRCODE 15    ; Reserved
ISR_NOERRCODE 16    ; x87 floating-point exception
ISR_ERRCODE   17    ; Alignment check (error code)
ISR_NOERRCODE 18    ; Machine check
ISR_NOERRCODE 19    ; SIMD floating-point exception
ISR_NOERRCODE 20    ; Virtualization exception
ISR_NOERRCODE 21    ; Reserved
ISR_NOERRCODE 22    ; Reserved
ISR_NOERRCODE 23    ; Reserved
ISR_NOERRCODE 24    ; Reserved
ISR_NOERRCODE 25    ; Reserved
ISR_NOERRCODE 26    ; Reserved
ISR_NOERRCODE 27    ; Reserved
ISR_NOERRCODE 28    ; Reserved
ISR_NOERRCODE 29    ; Reserved
ISR_ERRCODE   30    ; Security exception (error code)
ISR_NOERRCODE 31    ; Reserved

; Hardware IRQs (32-47)
IRQ 0,  32          ; Timer
IRQ 1,  33          ; Keyboard
IRQ 2,  34          ; Cascade
IRQ 3,  35          ; COM2
IRQ 4,  36          ; COM1
IRQ 5,  37          ; LPT2
IRQ 6,  38          ; Floppy
IRQ 7,  39          ; LPT1
IRQ 8,  40          ; CMOS RTC
IRQ 9,  41          ; Free
IRQ 10, 42          ; Free
IRQ 11, 43          ; Free
IRQ 12, 44          ; PS/2 Mouse
IRQ 13, 45          ; FPU
IRQ 14, 46          ; Primary ATA
IRQ 15, 47          ; Secondary ATA

; System Calls (INT 0x80)
ISR_NOERRCODE 128

; IDT Flush - LIDT instruction
global idt_flush
idt_flush:
    mov eax, [esp + 4]       ; IDT pointer adresini al
    lidt [eax]               ; IDT'yi IDTR'ye yükle
    ret
