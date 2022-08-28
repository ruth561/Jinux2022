#include "acpi.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;

namespace acpi
{
    bool RSDP::IsValid() const {
        if (strncmp(this->signature, "RSD PTR ", 8) != 0) {
            logger->error("Invalid Signature: %.8s\n", this->signature);
            return false;
        }
        if (this->revision != 2) {
            logger->error("ACPI Revision Must Be 2: %d\n", this->revision);
            return false;
        }
        // checksum
        uint8_t sum = 0;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(this);
        for (int i = 0; i < 20; i++) {
            sum += ptr[i];
        }
        if (sum != 0) {
            logger->error("Sum Of 20 bytes Must Be 0: %d\n", sum);
            return false;
        }
        // extended checksum
        sum = 0;
        for (int i = 0; i < 36; i++) {
            sum += ptr[i];
        }
        if (sum != 0) {
            logger->error("Sum Of 36 bytes Must Be 0: %d\n", sum);
            return false;
        }
        return true;
    }

    bool DescriptionHeader::IsValid(const char* expected_signature) const {
        if (strncmp(this->signature, expected_signature, 4) != 0) {
            logger->error("Invalid Signature: %.4s\n", this->signature);
            return false;
        }
        // checksum
        uint8_t sum = 0;
        const uint8_t *ptr = reinterpret_cast<const uint8_t *>(this);
        for (int i = 0; i < this->length; i++) {
            sum += ptr[i];
        }
        if (sum != 0) {
            logger->error("Sum Of %u bytes Must Be 0: %d\n", this->length,  sum);
            return false;
        }
        return true;
    }

    
    const FADT* fadt;

    void WaitMilliseconds(unsigned long msec) {
        const bool pm_timer_32 = (fadt->flags >> 8) & 1;
        // printk("PM_TMR_BLK: %x\n", fadt->pm_tmr_blk);
        const uint32_t start = ReadIOAddressSpace32(static_cast<uint16_t>(fadt->pm_tmr_blk));
        uint32_t end = start + kPMTimerFreq * msec / 1000;
        if (!pm_timer_32) {
            end &= 0x00ffffffu;
        }
        // printk("start: %x, end: %x\n", start, end);

        if (end < start) { // overflow
            while (ReadIOAddressSpace32(fadt->pm_tmr_blk) >= start);
        }
        while (ReadIOAddressSpace32(fadt->pm_tmr_blk) < end);
    }

    void Initialize(const RSDP *rsdp)
    {
        logging::LoggingLevel current_level = logger->current_level();
        logger->set_level(logging::kDEBUG);
        logger->debug("[ACPI] RSDP\n");
        logger->debug("[ACPI] Ptr: %p\n", rsdp);
        logger->debug("[ACPI] Signature: %c%c%c%c%c%c%c%c\n", 
            rsdp->signature[0], rsdp->signature[1], rsdp->signature[2], rsdp->signature[3], 
            rsdp->signature[4], rsdp->signature[5], rsdp->signature[6], rsdp->signature[7]);
        logger->debug("[ACPI] OEM: %c%c%c%c%c%c\n", 
            rsdp->oem_id[0], rsdp->oem_id[1], rsdp->oem_id[2], 
            rsdp->oem_id[3], rsdp->oem_id[4], rsdp->oem_id[5]);
        logger->debug("[ACPI] Revision: %d\n", rsdp->revision);
        logger->debug("[ACPI] Length: %d\n", rsdp->length);
        logger->debug("[ACPI] RSDT: %p\n", rsdp->rsdt_address);
        logger->debug("[ACPI] XSDT: %p\n", rsdp->xsdt_address);

        if (!rsdp->IsValid()) {
            return;
        }
        logger->info("[ACPI] RSDP Is Valid!\n");


        XSDT *xsdt = reinterpret_cast<XSDT *>(rsdp->xsdt_address);
        logger->debug("[ACPI] XSDT\n");
        logger->debug("[ACPI] Signature: %c%c%c%c\n", 
            xsdt->header.signature[0], xsdt->header.signature[1], 
            xsdt->header.signature[2], xsdt->header.signature[3]);
        logger->debug("[ACPI] Length: %d\n", xsdt->header.length);
        logger->debug("[ACPI] Revision: %d\n", xsdt->header.revision);
        logger->debug("[ACPI] OEM: %c%c%c%c%c%c\n", 
            xsdt->header.oem_id[0], xsdt->header.oem_id[1], 
            xsdt->header.oem_id[2], xsdt->header.oem_id[3], 
            xsdt->header.oem_id[4], xsdt->header.oem_id[5]);

        if (!xsdt->header.IsValid("XSDT")) {
            return;
        }
        logger->info("[ACPI] XSDT Is Valid!\n");

        for (int i = 0; i < xsdt->Size(); i++) {
            DescriptionHeader *entry = xsdt->entries[i];
            if (entry->IsValid("FACP")) {
                fadt = reinterpret_cast<const FADT*>(entry);
                logger->debug("FACP %p!!\n", fadt);
            }
        }

        logger->set_level(current_level);
    }
}