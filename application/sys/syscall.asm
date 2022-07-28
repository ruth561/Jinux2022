bits 64
section .text

global SyscallLogString
SyscallLogString:
    mov rax, 1
    mov r10, rcx
    syscall
    ret