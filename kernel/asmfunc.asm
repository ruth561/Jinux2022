
bits 64
section .text

extern kernel_main_stack
extern KernelMainNewStack

global KernelMain
; ここがカーネルのエントリポイントになる。
; UEFIのスタック領域をカーネルの領域に移す。
KernelMain:
    mov rsp, kernel_main_stack + 1024 * 1024
    call KernelMainNewStack
.fin:
    hlt
    jmp .fin

global GetCS  ; uint16_t GetCS(void);
GetCS:
    xor eax, eax  ; also clears upper 32 bits of rax
    mov ax, cs
    ret
    
global LoadIDT  ; void LoadIDT(uint16_t limit, uint64_t offset);
LoadIDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di  ; limit
    mov [rsp + 2], rsi  ; offset
    lidt [rsp]
    mov rsp, rbp
    pop rbp
    ret

global LoadGDT
LoadGDT:
    push rbp
    mov rbp, rsp
    sub rsp, 10
    mov [rsp], di
    mov [rsp + 2], rsi
    lgdt [rsp]
    mov rsp, rbp
    pop rbp
    ret

global SetCSSS  ; void SetCSSS(uint16_t cs, uint16_t ss);
SetCSSS:
    push rbp
    mov rbp, rsp
    mov ss, si
    mov rax, .next
    push rdi    ; CS
    push rax    ; RIP
    o64 retf
.next:
    mov rsp, rbp
    pop rbp
    ret


global SetDSAll  ; void SetDSAll(uint16_t value);
SetDSAll:
    mov ds, di
    mov es, di
    mov fs, di
    mov gs, di
    ret

global SetCR3   ; void SetCR3(uint64_t value);
SetCR3:
    mov cr3, rdi
    ret

;
; GetCRn
;
global GetCR0   ; uint64_t GetCR0();
GetCR0:
    mov rax, cr0
    ret

global GetCR2   ; uint64_t GetCR2();
GetCR2:
    mov rax, cr2
    ret

global GetCR3   ; uint64_t GetCR3();
GetCR3:
    mov rax, cr3
    ret

global GetCR4   ; uint64_t GetCR4();
GetCR4:
    mov rax, cr4
    ret

global ReadFromLinearAddress    ; uint64_t ReadFromLinearAddress(void *lin_addr);
ReadFromLinearAddress:
    mov rax, [rdi]
    ret
