#include "interrupt.hpp"
#include "logging.hpp"
extern logging::Logger *logger;




// Interrupt Descriptor Table
alignas(16) std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(int vector_number, 
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

//
// ０除算の例外のハンドラ（IDT[0]）
// とりあえずはhltするようにしておく。
//
__attribute__((interrupt))
void DivideErrorHandler(void *frame)
{
    logger->error("[!-- EXCEPTION --!] 0 divide error exception detected!\n");
    __asm__("hlt");
}

//
// ページフォルトの例外ハンドラ（IDT[14]）
//
//
__attribute__((interrupt))
void PageFaultHandler(void *frame)
{
    // CR2にフォルトしたリニアアドレスが渡される
    logger->error("[!-- EXCEPTION --!] PAGE FAULT at 0x%lx!\n", GetCR2());
    __asm__("hlt");
}

void SetupInterruptDescriptorTable()
{
    logger->debug("idt: %p\n", &idt);
    const uint16_t cs = GetCS();

    // IDTへゲートを追加してゆく
    logger->info("Setting IDT[0]\n"); // ０除算例外
    SetIDTEntry(0, reinterpret_cast<uintptr_t>(DivideErrorHandler), cs, 15);

    logger->info("Setting IDT[14]\n"); // ページフォルト
    SetIDTEntry(14, reinterpret_cast<uintptr_t>(PageFaultHandler), cs, 15);


    // idtrレジスタを新しいテーブルのアドレスに書き換える。
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
