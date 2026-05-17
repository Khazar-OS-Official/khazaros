; Khazar OS Bootloader - Real Hardware Compatible
; Multiboot Specification 0.6.96 uyumlu
;
; KRİTİK DÜZELTME:
;   Stack `.bss` içindedir. BSS temizleme (rep stosb) stack'i de sıfırlar.
;   Bu yüzden magic ve mbi değerlerini STACK'E DEĞİL, .data değişkenlerine sakla.
;
; FLAGS:
;   Bit 0 = ALIGN    : modülleri 4KB'a hizala
;   Bit 1 = MEM_INFO : bellek haritası isteği
;   Bit 2 = VIDEO    : video modu isteği (grub.cfg + multiboot header)

MAGIC    equ 0x1BADB002
FLAGS    equ 0x00000007   ; ALIGN + MEMINFO + VIDEO
CHECKSUM equ -(MAGIC + FLAGS)

KERNEL_VIRTUAL_BASE equ 0xC0000000

; ─── Multiboot Header ─────────────────────────────────────────────────────────
; Spec: ilk 8KB içinde, 4-byte hizalı olmalı
; FLAGS bit16=0 olduğundan address fields YOK.
; Video fields (bit2=1) checksumdan hemen sonra gelir: offset 12'de.
SECTION .multiboot
ALIGN 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; Video mode fields - bit16=0 olduğundan offset 12'de başlar (address fields yok!)
    dd 0                ; mode_type: 0=linear RGB framebuffer
    dd 1024             ; width  (0=any, ama 1024 tercih edilir)
    dd 768              ; height (0=any, ama 768 tercih edilir)
    dd 32               ; bpp

; ─── .data: Boot Page Tables + Magic/MBI Saklama ──────────────────────────────
; NOT: Bunlar .bss değil .data içinde çünkü BSS clear sırasında korunmalılar.
SECTION .data
ALIGN 4096
boot_page_directory:
    times 1024 dd 0

; BSS temizleme stack'i sıfırladığından magic ve mbi'yi buraya kaydediyoruz
ALIGN 4
saved_magic: dd 0
saved_mbi:   dd 0

; ─── Stack: .data içinde (BSS'den ayri, temizlenmeyecek) ──────────────────────
; ÖNEMLİ: Stack .bss içinde olursa BSS clear onu sıfırlar!
; Bu yüzden stack'i .data içine alıyoruz.
ALIGN 16
kernel_stack:
    times 16384 db 0    ; 16KB stack (.data içinde, initialized)
kernel_stack_top:

; ─── .bss: Sadece diğer C değişkenleri için ───────────────────────────────────
SECTION .bss
ALIGN 16
bss_placeholder:

; ─── Giriş Noktası ────────────────────────────────────────────────────────────
SECTION .text
global _start
_start:
    ; Paging KAPALI. EAX=magic, EBX=mbi (fiziksel adres)
    ; Erken debug: fiziksel VGA adresine direkt yaz
    mov word [0xB8000], 0x0F4B   ; 'K' beyaz - Kernel başladı

    ; Magic ve MBI'yi geçici kayıtlarda sakla
    mov ecx, eax    ; magic -> ecx
    mov edx, ebx    ; mbi   -> edx

    ; ─── Enable PSE (Page Size Extension) for 4MB pages ──────────────────────
    mov eax, cr4
    or  eax, 0x00000010
    mov cr4, eax

    ; ─── Page Directory: Identity (0-4GB) + Higher-Half (0xC0000000) ─────────
    ; 1024 entries * 4MB = 4GB. Both identity map and higher half map are easy!
    
    ; 1. Identity Map 0-4GB (entries 0 to 1023)
    mov edi, boot_page_directory - KERNEL_VIRTUAL_BASE
    mov eax, 0x00000083         ; Present + Writable + 4MB Page
    mov ebx, 1024               ; 1024 entries
.fill_id:
    stosd
    add eax, 0x00400000         ; Add 4MB to physical address
    dec ebx
    jnz .fill_id

    ; 2. Higher-Half Map 0xC0000000 -> 0x00000000 (entries 768 to 1023)
    ; virtual 0xC0000000 maps to physical 0x00000000
    mov edi, boot_page_directory - KERNEL_VIRTUAL_BASE + (768 * 4)
    mov eax, 0x00000083         ; Present + Writable + 4MB Page
    mov ebx, 256                ; 256 entries (1GB)
.fill_hh:
    stosd
    add eax, 0x00400000         ; Add 4MB to physical address
    dec ebx
    jnz .fill_hh

    ; ─── Paging Aktif ────────────────────────────────────────────────────────
    mov eax, boot_page_directory - KERNEL_VIRTUAL_BASE
    mov cr3, eax
    mov eax, cr0
    or  eax, 0x80010000     ; PG=1, WP=1
    mov cr0, eax

    ; Higher-half'e atla (virtual adresler artık geçerli)
    lea eax, [higher_half]
    jmp eax

higher_half:
    ; Artık virtual adresler kullanılabilir
    mov word [0xC00B8002], 0x0F48   ; 'H' - Higher-half OK

    ; Stack'i kur (.data içindeki stack, BSS temizlemeden etkilenmez)
    mov esp, kernel_stack_top

    ; ─── Magic ve MBI'yi .data değişkenlerine kaydet ─────────────────────────
    ; NEDEN: BSS temizleme rep stosb ile bss_start..bss_end arasını sıfırlar.
    ; Stack .data içinde olsa da, push/pop yerine direct store güvenli.
    mov [saved_magic], ecx
    mov [saved_mbi],   edx

    ; ─── BSS Sıfırla ─────────────────────────────────────────────────────────
    extern bss_start
    extern bss_end
    mov edi, bss_start
    xor eax, eax
    mov ecx, bss_end
    sub ecx, edi
    test ecx, ecx
    jz  .bss_done
    rep stosb
.bss_done:

    ; ─── kernel_main Çağır ────────────────────────────────────────────────────
    ; .data'dan oku (BSS temizleme bunları bozmadı)
    mov ecx, [saved_magic]
    mov edx, [saved_mbi]

    ; cdecl: son parametre önce push edilir
    push edx    ; arg2: mbi_addr
    push ecx    ; arg1: magic
    extern kernel_main
    call kernel_main

    ; Buraya hiç gelmemeli
.halt:
    cli
    hlt
    jmp .halt
