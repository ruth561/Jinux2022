#include <deque>

#include "interrupt.hpp"
#include "timer.hpp"
#include "logging.hpp"
#include "message.hpp"

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

/* 
 * ０除算の例外のハンドラ（IDT[0]）
 * とりあえずはhltするようにしておく。 
 */
__attribute__((interrupt))
void DivideErrorHandler(void *frame)
{
    logger->error("[!-- EXCEPTION --!] 0 divide error exception detected!\n");
    __asm__("hlt");
}

/* 
 * ページフォルトの例外ハンドラ（IDT[14]） 
 */
__attribute__((interrupt))
void PageFaultHandler(void *frame)
{
    // CR2にフォルトしたリニアアドレスが渡される
    logger->error("[!-- EXCEPTION --!] PAGE FAULT at 0x%lx!\n", GetCR2());
    __asm__("hlt");
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


    // IDTへゲートを追加してゆく
    logger->info("Setting IDT[%02xh] kDivideError\n", InterruptVector::kDivideError); // ０除算例外
    SetIDTEntry(InterruptVector::kDivideError, reinterpret_cast<uintptr_t>(DivideErrorHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kPageFault\n", InterruptVector::kPageFault); // ページフォルト
    SetIDTEntry(InterruptVector::kPageFault, reinterpret_cast<uintptr_t>(PageFaultHandler), cs, 15);

    logger->info("Setting IDT[%02xh] kLAPICTimer\n", InterruptVector::kLAPICTimer);
    SetIDTEntry(InterruptVector::kLAPICTimer, reinterpret_cast<uintptr_t>(IntHandlerLAPICTimer), cs, 14);


    // idtrレジスタを新しいテーブルのアドレスに書き換える。
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));

    // RFLAGSの割り込み許可フラグをセットし割り込みを受け付ける。
    __asm__("sti");
}
