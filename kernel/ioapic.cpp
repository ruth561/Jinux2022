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

    void Initialize()
    {
        logging::LoggingLevel current_level = logger->current_level();
        logger->set_level(logging::kDEBUG);
        logger->debug("IO APIC ID:          %08x\n", Read(0));
        logger->debug("IO APIC VERSION:     %08x\n", Read(1));
        logger->debug("IO APIC ARBITRATION: %08x\n", Read(2));





        logger->set_level(current_level);
    }
}
