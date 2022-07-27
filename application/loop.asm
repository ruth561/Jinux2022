

bits 64
section .text

;    push rax
;    pop rax
    mov eax, 0
    mov r10, rcx
    mov rdi, 0x41424344454647
    syscall
loop:
    mov rax, 0xdeadbeef
    mov rbx, 0xcafebabe
    push rax
    push rbx
    pop rbx
    pop rax
    jmp loop

