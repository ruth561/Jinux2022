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
int printk(const char *format, ...);


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
 * #PF(14)
 * ページフォルトはデマンドページング実装のために特殊になっている
 * ！！例外内ではvsprintfは使用できないので、FORMAT文字列による出力は避けるべし。！！
 */
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
    
    printk("[!-- EXCEPTION --!] #PF\n");
    printk("  "); printk(error_code.bits.caused_by_page_level_protection ? "PAGE-LEVEL PROTECTION VIOLATION" : "NON-PRESENT PAGE ACCESS"); printk("\n");
    printk("  "); printk(error_code.bits.u_s ? "USER " : "SUPERVISOR "); printk("\n");
    printk("  "); printk(error_code.bits.w_r ? "WRITE" : "READ"); printk("\n");
    if (error_code.bits.rsvd)
        printk("  THE FAULT WAS CAUSED BY A RESERVED BIT SET TO 1 IN SOME PAGING-STRUCURE ENTRY.\n");
    if (error_code.bits.caused_by_instruction_fetch)
        printk("  THE FAULT WAS CAUSED BY AN INSTRUCTION FETCH.\n");

    while (1) __asm__("hlt");
}


// XHCIの割り込みハンドラ
__attribute__((interrupt)) 
void IntHandlerXHCI(InterruptFrame *frame)
{
    printk("[!-- INTERRUPT --!] xHCI\n");
    NotifyEndOfInterrupt();
}

// エラーコードありの例外ハンドラ
#define FaultHandlerWithError(fault_name) \
    __attribute__((interrupt)) \
    void IntHandler ## fault_name (InterruptFrame* frame, uint64_t error_code) { \
        printk("[!-- EXCEPTION --!] #"); printk(#fault_name); printk("\n"); \
        while (true) __asm__("hlt"); \
    }

// エラーコードなしの例外ハンドラ
#define FaultHandlerNoError(fault_name) \
    __attribute__((interrupt)) \
    void IntHandler ## fault_name (InterruptFrame* frame) { \
        printk("[!-- EXCEPTION --!] #"); printk(#fault_name); printk("\n"); \
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

    logger->info("Setting IDT[%02xh] kPageFault\n", InterruptVector::kPageFault); // ページフォルト
    SetIDTEntry(InterruptVector::kPageFault, reinterpret_cast<uintptr_t>(PageFaultHandler), cs, 14, kISTForPF); // 例外ハンドラだが、割り込みを受け付けないようにInterruptGateにしている。

    logger->info("Setting IDT[%02xh] kXHCI\n", InterruptVector::kXHCI); // xHCI
    SetIDTEntry(InterruptVector::kXHCI, reinterpret_cast<uintptr_t>(IntHandlerXHCI), cs, 14, kISTForTimer);

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
