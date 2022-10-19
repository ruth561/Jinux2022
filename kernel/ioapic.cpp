#include "ioapic.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;


namespace ioapic
{
    #define IOAPICID          0x00
    #define IOAPICVER         0x01
    #define IOAPICARB         0x02
    #define IOAPICREDTBL(n)   (0x10 + 2 * n)

    volatile uint32_t *io_register_select = reinterpret_cast<uint32_t *>(0xfec00000lu);
    volatile uint32_t *io_window = reinterpret_cast<uint32_t *>(0xfec00010lu);

    void Write(uint8_t index, uint32_t data)
    {
        *io_register_select = static_cast<uint32_t>(index);
        *io_window = static_cast<uint32_t>(data);
    }

    uint32_t Read(uint8_t index)
    {
        *io_register_select = static_cast<uint32_t>(index);
        return *io_window;
    }

    void WriteRedirectionTable(uint8_t ent_num, RedirectionTableEntry entry)
    {
        Write(IOAPICREDTBL(ent_num), entry.data[0]);     // 下位32bit
        Write(IOAPICREDTBL(ent_num) + 1, entry.data[1]); // 上位32bit
    }

    RedirectionTableEntry ReadRedirectionTable(uint8_t ent_num)
    {
        RedirectionTableEntry entry;
        entry.data[0] = Read(IOAPICREDTBL(ent_num));
        entry.data[1] = Read(IOAPICREDTBL(ent_num) + 1);
        return entry;
    }

    void Initialize()
    {
        logging::LoggingLevel current_level = logger->current_level();
        logger->set_level(logging::kDEBUG);
        logger->debug("IO APIC ID:          %08x\n", Read(IOAPICID));
        logger->debug("IO APIC VERSION:     %08x\n", Read(IOAPICVER));
        logger->debug("IO APIC ARBITRATION: %08x\n", Read(IOAPICARB));

        RedirectionTableEntry entry;
        entry.data[0] = 0;
        entry.data[1] = 0;
        /* for (uint8_t i = 16; i < 24; i++) {
            entry.bits.interrupt_vector = InterruptVector::kLegacyInterruptFromIOAPIC;
            // 割り込みは物理的な０番目のプロセッサに運ばれる
            entry.bits.destination_mode = 0;
            entry.bits.destination_field = 0; 

            entry.bits.delivery_mode = 0; // fixed mode
            entry.bits.interrupt_mask = false; // not masked 

            entry.bits.interrupt_input_pin_polarity = false; // asserted when low
            entry.bits.trigger_mode = true; // edge mode

            WriteRedirectionTable(i, entry);
            entry = ReadRedirectionTable(i);
            logger->debug("REDIRECTION TABLE %02d: %08x %08x\n", i, entry.data[0], entry.data[1]);
        } */

        entry = ReadRedirectionTable(11); // rtl8139
        logger->debug("REDIRECTION TABLE %02d: %08x %08x\n", 11, entry.data[0], entry.data[1]);
        entry.data[0] = 0;
        entry.data[1] = 0;
        entry.bits.interrupt_vector = InterruptVector::kLegacyInterruptFromIOAPIC;
        entry.bits.interrupt_input_pin_polarity = true; // asserted when high
        entry.bits.destination_field = 0;
        
        WriteRedirectionTable(11, entry); // rtl8139
        entry = ReadRedirectionTable(11);
        logger->debug("REDIRECTION TABLE %02d: %08x %08x\n", 11, entry.data[0], entry.data[1]);
        // Halt();




        logger->set_level(current_level);
        // Halt();
    }
}
