bits 64
section .text

global SyscallExit
SyscallExit:
    mov rax, 0
    mov r10, rcx
    syscall
    ret ; ここには帰ってこない、、ハズ、、、

global SyscallLogString
SyscallLogString:
    mov rax, 1
    mov r10, rcx
    syscall
    ret
