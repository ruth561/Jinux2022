#include <deque>

#include "interrupt.hpp"
#include "timer.hpp"
#include "logging.hpp"
#include "message.hpp"
#include "memory_manager.hpp"

extern logging::Logger *logger;
extern TimerManager *timer_manager;
extern std::deque<Message> *main_queue;
extern TaskManager* task_manager;


// 割り込みディスクリプタテーブル
alignas(16) std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(InterruptVector vector_number, 
                 uintptr_t offset, 
                 uint16_t segment_selector, 
                 uint16_t type, 
                 uint16_t interrupt_stack_table, 
                 uint16_t descriptor_priviledge_level, 
                 uint16_t segment_present_flag) {
    idt[vector_number].SetOffset(offset);
    idt[vector_number].segment_selector = segment_selector;
    idt[vector_number].interrupt_stack_table = interrupt_stack_table;
    idt[vector_number].type = type;
    idt[vector_number].descriptor_priviledge_level = descriptor_priviledge_level;
    idt[vector_number].segment_present_flag = segment_present_flag;
}


/* 
 * 割り込みが終了したことを伝える関数 
 */
void NotifyEndOfInterrupt() {
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
    *end_of_interrupt = 0;
}

// #DE(00)
__attribute__((interrupt))
void DivideErrorHandler(void *frame)
{
    logger->error("[!-- EXCEPTION --!] 0 divide error exception detected!\n");
    __asm__("hlt");
}

// #UD(06)
// 今の所、機能しているかよくわからない、、
__attribute__((interrupt))
void InvalidOpecodeHandler(InterruptFrame *frame)
{
    logger->error("[!-- EXCEPTION --!] INVALID OPCODE EXCEPTION! PROCESS %ld.\n", 
        task_manager->CurrentTask()->ID());
    logger->error("RIP=0x%lx, CS=0x%lx, RFLAGS=0x%lx, RSP=0x%lx, SS=0x%lx\n",
        frame->rip, frame->cs, frame->rflags, frame->rsp, frame->ss);
        
    __asm__("sti");
    while (1) __asm__("hlt");
}

// #DF(08)
// 例外ハンドラの実行中に例外が検知されたことを示す例外。
// 本来、例外ハンドラを呼び出している最中に更に例外が起きた場合は、
// ２つの例外をハンドルしなければならないが、その時はそれら２つの例外はハンドルせず
// この例外を処理することになっている。
// 例外の種類は「Abort」であり、プログラムの復帰はできないようになっている。
__attribute__((interrupt))
void DoubleFaultHandler(InterruptFrame *frame, uint64_t error_code)
{
    logger->error("[!-- EXCEPTION --!] DOUBLE FAULT! PROCESS %ld\n", 
        task_manager->CurrentTask()->ID());
        
    __asm__("cli"); // OSを止める
    while (1) __asm__("hlt");
}

// #NP(11)
__attribute__((interrupt))
void SegmentNotPresentHandler(InterruptFrame *frame, uint64_t error_code)
{
    logger->error("[!-- EXCEPTION --!] SEGMENT NOT PRESENT!\n");
    while (1) __asm__("hlt");
}

// #GP(13)
__attribute__((interrupt))
void GeneralProtectionHandler(InterruptFrame *frame, uint64_t error_code)
{
    logger->error("[!-- EXCEPTION --!] GENERAL PROTECTION!\n");
    logger->error("ERROR_CODE=0x%lx, RIP=0x%lx, CS=0x%lx, RFLAGS=0x%lx, RSP=0x%lx, SS=0x%lx\n",
        error_code, frame->rip, frame->cs, frame->rflags, frame->rsp, frame->ss);
    __asm__("sti");
    while (true) {
        __asm__("hlt");
    }
}

// #PF(14)
__attribute__((interrupt)) 
void PageFaultHandler(InterruptFrame *frame, uint64_t error_code_)
{
    // DebugFunc();
    // __asm__("cli");
    // CR2にフォルトしたリニアアドレスが渡される
    PageFaultErrorCode error_code;
    error_code.data = error_code_;
    uint64_t error_address = GetCR2();

    int res = HandlePageFault(error_code, error_address);
    if (res == 0) {
        // __asm__("sti");
        return;
    }
    
    logger->error("[!-- EXCEPTION --!] PAGE FAULT at 0x%lx!\n", error_address);
    logger->error("    %s.\n", 
        error_code.bits.caused_by_page_level_protection ? "PAGE-LEVEL PROTECTION VIOLATION" : "NON-PRESENT PAGE ACCESS");
    logger->error("    %s %s.\n", 
        error_code.bits.u_s ? "USER" : "SUPERVISOR",
        error_code.bits.w_r ? "WRITE" : "READ");
    
    if (error_code.bits.rsvd)
        logger->error("    THE FAULT WAS CAUSED BY A RESERVED BIT SET TO 1 IN SOME PAGING-STRUCURE ENTRY.\n");

    if (error_code.bits.caused_by_instruction_fetch)
        logger->error("    THE FAULT WAS CAUSED BY AN INSTRUCTION FETCH.\n");

    // __asm__("sti");
    while (1);
}


int printk(const char *format, ...);

// エラーコードありの例外ハンドラ
#define FaultHandlerWithError(fault_name) \
    __attribute__((interrupt)) \
    void IntHandler ## fault_name (InterruptFrame* frame, uint64_t error_code) { \
        printk("[!-- EXCEPTION --!] #%s\n", #fault_name); \
        while (true) __asm__("hlt"); \
    }

// エラーコードなしの例外ハンドラ
#define FaultHandlerNoError(fault_name) \
    __attribute__((interrupt)) \
    void IntHandler ## fault_name (InterruptFrame* frame) { \
        printk("[!-- EXCEPTION --!] #%s\n", #fault_name); \
        while (true) __asm__("hlt"); \
    }

FaultHandlerNoError(DE)
FaultHandlerNoError(DB)
FaultHandlerNoError(BP)
FaultHandlerNoError(OF)
FaultHandlerNoError(BR)
FaultHandlerNoError(UD)
FaultHandlerNoError(NM)
FaultHandlerWithError(DF)
FaultHandlerWithError(TS)
FaultHandlerWithError(NP)
FaultHandlerWithError(SS)
FaultHandlerWithError(GP)
// FaultHandlerWithError(PF)
FaultHandlerNoError(MF)
FaultHandlerWithError(AC)
FaultHandlerNoError(MC)
FaultHandlerNoError(XM)
FaultHandlerNoError(VE)


void SetupInterruptDescriptorTable()
{
    logger->info("[+] Setup IDT\n");
    logger->debug("idt: %p\n", &idt);
    const uint16_t cs = GetCS();


    // IDTへゲートを追加していく    
    logger->info("Setting IDT[%02xh] kDivideError\n", InterruptVector::kDivideError); // ０除算例外
    SetIDTEntry(InterruptVector::kDivideError, reinterpret_cast<uintptr_t>(DivideErrorHandler), cs, 14);

    logger->info("Setting IDT[%02xh] kInvalidOpecode\n", InterruptVector::kInvalidOpecode); // 無効命令
    SetIDTEntry(InterruptVector::kInvalidOpecode, reinterpret_cast<uintptr_t>(InvalidOpecodeHandler), cs, 14);

    logger->info("Setting IDT[%02xh] kDoubleFault\n", InterruptVector::kDoubleFault); // 例外ハンドラ中の例外
    SetIDTEntry(InterruptVector::kDoubleFault, reinterpret_cast<uintptr_t>(DoubleFaultHandler), cs, 14);

    logger->info("Setting IDT[%02xh] kSegmentNotPresent\n", InterruptVector::kSegmentNotPresent); // セグメントの不在
    SetIDTEntry(InterruptVector::kSegmentNotPresent, reinterpret_cast<uintptr_t>(SegmentNotPresentHandler), cs, 14);

    logger->info("Setting IDT[%02xh] kGeneralProtection\n", InterruptVector::kGeneralProtection); // 一般保護例外
    SetIDTEntry(InterruptVector::kGeneralProtection, reinterpret_cast<uintptr_t>(GeneralProtectionHandler), cs, 14, kISTForGP);

    logger->info("Setting IDT[%02xh] kPageFault\n", InterruptVector::kPageFault); // ページフォルト
    SetIDTEntry(InterruptVector::kPageFault, reinterpret_cast<uintptr_t>(PageFaultHandler), cs, 14, kISTForPF); // 例外ハンドラだが、割り込みを受け付けないようにInterruptGateにしている。

    logger->info("Setting IDT[%02xh] kLAPICTimer\n", InterruptVector::kLAPICTimer);
    SetIDTEntry(InterruptVector::kLAPICTimer, reinterpret_cast<uintptr_t>(IntHandlerLAPICTimer), cs, 14, kISTForTimer);

    auto set_idt_entry = [](int vector_number, auto handler) {
        idt[vector_number].SetOffset(reinterpret_cast<uintptr_t>(handler));
        idt[vector_number].segment_selector = kKernelCS;
        idt[vector_number].interrupt_stack_table = 2;
        idt[vector_number].type = 14;
        idt[vector_number].descriptor_priviledge_level = 0;
        idt[vector_number].segment_present_flag = 1;
    };
    set_idt_entry(0,  IntHandlerDE);
    set_idt_entry(1,  IntHandlerDB);
    set_idt_entry(3,  IntHandlerBP);
    set_idt_entry(4,  IntHandlerOF);
    set_idt_entry(5,  IntHandlerBR);
    set_idt_entry(6,  IntHandlerUD);
    set_idt_entry(7,  IntHandlerNM);
    set_idt_entry(8,  IntHandlerDF);
    set_idt_entry(10, IntHandlerTS);
    set_idt_entry(11, IntHandlerNP);
    set_idt_entry(12, IntHandlerSS);
    set_idt_entry(13, IntHandlerGP);
    // set_idt_entry(14, IntHandlerPF);
    set_idt_entry(16, IntHandlerMF);
    set_idt_entry(17, IntHandlerAC);
    set_idt_entry(18, IntHandlerMC);
    set_idt_entry(19, IntHandlerXM);
    set_idt_entry(20, IntHandlerVE);
    // idtrレジスタを新しいテーブルのアドレスに書き換える。
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

    // RFLAGSの割り込み許可フラグをセットし割り込みを受け付ける。
    __asm__("sti");
}
