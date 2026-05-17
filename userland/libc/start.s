; Userland Entry Point
; Links with every user PE binary
; Calls main() and passes the result to exit()

global __start
global _start
extern _start_c
extern _exit

section .text
__start:
_start:
    ; Align stack
    and esp, 0xFFFFFFF0
    
    ; Hand execution over to C wrapper for argc/argv parsing
    call _start_c
    
    ; Exit should not return, but just in case
.hang:
    jmp .hang
