#include <deque>

#include "interrupt.hpp"
#include "timer.hpp"
#include "logging.hpp"
#include "message.hpp"
#include "memory_manager.hpp"

extern logging::Logger *logger;
extern TimerManager *timer_manager;
extern std::deque<Message> *main_queue;


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
void InvalidOpecodeHandler(void *frame)
{
    logger->error("[!-- EXCEPTION --!] INVALID OPCODE EXCEPTION!\n");
    __asm__("hlt");
}

// #NP(11)
__attribute__((interrupt))
void SegmentNotPresentHandler(InterruptFrame *frame, uint64_t error_code)
{
    logger->error("[!-- EXCEPTION --!] SEGMENT NOT PRESENT!\n");
    __asm__("hlt");
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
    // CR2にフォルトしたリニアアドレスが渡される
    PageFaultErrorCode error_code;
    error_code.data = error_code_;
    uint64_t error_address = GetCR2();

    int res = HandlePageFault(error_code, error_address);
    if (res == 0) {
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

    while (1);
}

/* 
 * Local APIC Timerの割り込みハンドラ 
 */
__attribute__((interrupt))
void IntHandlerLAPICTimer(void *frame)
{
    LAPICTimerOnInterrupt(); // timer.cppで定義
}


void SetupInterruptDescriptorTable()
{
    logger->info("[+] Setup IDT\n");
    logger->debug("idt: %p\n", &idt);
    const uint16_t cs = GetCS();


    // IDTへゲートを追加していく    
    logger->info("Setting IDT[%02xh] kDivideError\n", InterruptVector::kDivideError); // ０除算例外
    SetIDTEntry(InterruptVector::kDivideError, reinterpret_cast<uintptr_t>(DivideErrorHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kInvalidOpecode\n", InterruptVector::kInvalidOpecode); // 無効命令
    SetIDTEntry(InterruptVector::kInvalidOpecode, reinterpret_cast<uintptr_t>(InvalidOpecodeHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kSegmentNotPresent\n", InterruptVector::kSegmentNotPresent); // セグメントの不在
    SetIDTEntry(InterruptVector::kSegmentNotPresent, reinterpret_cast<uintptr_t>(SegmentNotPresentHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kGeneralProtection\n", InterruptVector::kGeneralProtection); // 一般保護例外
    SetIDTEntry(InterruptVector::kGeneralProtection, reinterpret_cast<uintptr_t>(GeneralProtectionHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kPageFault\n", InterruptVector::kPageFault); // ページフォルト
    SetIDTEntry(InterruptVector::kPageFault, reinterpret_cast<uintptr_t>(PageFaultHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kLAPICTimer\n", InterruptVector::kLAPICTimer);
    SetIDTEntry(InterruptVector::kLAPICTimer, reinterpret_cast<uintptr_t>(IntHandlerLAPICTimer), cs, 14);


    // idtrレジスタを新しいテーブルのアドレスに書き換える。
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

    // RFLAGSの割り込み許可フラグをセットし割り込みを受け付ける。
    __asm__("sti");
}
