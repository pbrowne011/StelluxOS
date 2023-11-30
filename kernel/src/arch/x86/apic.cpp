#include "apic.h"
#include "msr.h"
#include <paging/page.h>
#include <memory/kmemory.h>
#include <paging/tlb.h>
#include <kelevate/kelevate.h>

volatile uint32_t* g_lapicBase = NULL;
uint64_t g_lapicPhysicalBase = 0;

void initializeApic() {
    uint64_t apicBaseMsr;

    if (g_lapicBase != NULL) {
        return;
    }

    RUN_ELEVATED({
        apicBaseMsr = readMsr(IA32_APIC_BASE_MSR);
    });

    // Enable APIC by setting the 11th bit
    apicBaseMsr |= (1 << 11);

    RUN_ELEVATED({
        writeMsr(IA32_APIC_BASE_MSR, apicBaseMsr);
    });

    g_lapicBase = (uint32_t*)(apicBaseMsr & ~0xFFF);

    g_lapicPhysicalBase = (uint64_t)g_lapicBase;

    // Map the LAPIC base into the kernel's address space
    void* lapicVirtualBase = zallocPage(); // this will lock a kernel page for our use

    RUN_ELEVATED({
        paging::mapPage(
            lapicVirtualBase,
            (void*)g_lapicBase,
            USERSPACE_PAGE,
            paging::g_kernelRootPageTable,
            paging::getGlobalPageFrameAllocator()
        );
        paging::flushTlbAll();
    });

    g_lapicBase = (uint32_t*)lapicVirtualBase;

    // Set the spurious interrupt vector and enable the APIC
    uint32_t spurious_vector = readApicRegister(0xF0);
    spurious_vector |= (1 << 8);    // Enable the APIC
    spurious_vector |= 0xFF;        // Set the spurious interrupt vector (choose a free vector number)
    writeApicRegister(0xF0, spurious_vector);
}

void* getApicBase() {
    return (void*)g_lapicBase;
}

uint64_t getLocalApicPhysicalBase() {
    return g_lapicPhysicalBase;
}

void completeApicIrq() {
    writeApicRegister(0xB0, 0);
}

void writeApicRegister(
    uint32_t reg,
    uint32_t value
) {
    g_lapicBase[reg / 4] = value;
}

uint32_t readApicRegister(
    uint32_t reg
) {
    return g_lapicBase[reg / 4];
}

void sendIpi(uint8_t apic_id, uint32_t vector) {
    writeApicRegister(APIC_ICR_HI, apic_id << 24);
    writeApicRegister(APIC_ICR_LO, vector | (1 << 14));
}
