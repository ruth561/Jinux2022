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

    void Initialize(const RSDP *rsdp)
    {
        logging::LoggingLevel current_level = logger->current_level();
        logger->set_level(logging::kDEBUG);
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
        logger->debug("RSDP Is Valid!\n");
        Halt();

        logger->set_level(current_level);
    }
}