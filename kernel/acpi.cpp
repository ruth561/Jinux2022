#include "acpi.hpp"
int printk(const char *format, ...);
void Halt();
extern logging::Logger *logger;

namespace acpi
{
    void Initialize(const RSDP *rsdp)
    {
        printk("[ACPI] Ptr: %p\n", rsdp);
        printk("[ACPI] Signature: %c%c%c%c%c%c%c%c\n", 
            rsdp->signature[0], rsdp->signature[1], rsdp->signature[2], rsdp->signature[3], 
            rsdp->signature[4], rsdp->signature[5], rsdp->signature[6], rsdp->signature[7]);

        Halt();
    }
}