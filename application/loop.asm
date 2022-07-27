

bits 64
section .text

    
loop:
    mov rax, 0xdeadbeef
    mov rbx, 0xcafebabe
    push rax
    push rbx
    pop rbx
    pop rax
    jmp loop

