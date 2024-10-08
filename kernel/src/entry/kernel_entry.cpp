#include "entry_params.h"
#include <memory/kmemory.h>
#include <graphics/kdisplay.h>
#include <gdt/gdt.h>
#include <paging/phys_addr_translation.h>
#include <paging/page.h>
#include <ports/serial.h>
#include <paging/tlb.h>
#include <interrupts/idt.h>
#include <arch/x86/cpuid.h>
#include <arch/x86/msr.h>
#include <arch/x86/apic.h>
#include <arch/x86/ioapic.h>
#include <arch/x86/apic_timer.h>
#include <arch/x86/gsfsbase.h>
#include <arch/x86/pat.h>
#include <arch/x86/ap_startup.h>
#include <sched/sched.h>
#include <syscall/syscalls.h>
#include <kelevate/kelevate.h>
#include <acpi/acpi_controller.h>
#include <time/ktime.h>
#include <kprint.h>

#include "tests/kernel_entry_tests.h"

// #define KE_TEST_MULTITHREADING
// #define KE_TEST_XHCI_INIT
#define KE_TEST_AP_STARTUP
// #define KE_TEST_CPU_TEMP_READINGS
// #define KE_TEST_PRINT_CURRENT_TIME
// #define KE_TEST_GRAPHICS

EXTERN_C __PRIVILEGED_CODE void _kentry(KernelEntryParams* params);
extern uint64_t __kern_phys_base;

extern uint64_t __ksymstart;
extern uint64_t __ksymend;

KernelEntryParams g_kernelEntryParameters;

#define USERMODE_KERNEL_ENTRY_STACK_SIZE 0x8000
char __usermodeKernelEntryStack[USERMODE_KERNEL_ENTRY_STACK_SIZE];

void _kuser_entry();

__PRIVILEGED_CODE void _kentry(KernelEntryParams* params) {
    // Setup kernel stack
    uint64_t kernelStackTop = reinterpret_cast<uint64_t>(params->kernelStack) + PAGE_SIZE;
    asm volatile ("mov %0, %%rsp" :: "r"(kernelStackTop));

    // Copy the kernel parameters to an unprivileged region
    memcpy(&g_kernelEntryParameters, params, sizeof(KernelEntryParams));

    // First thing we have to take care of
    // is setting up the Global Descriptor Table.
    initializeAndInstallGDT(BSP_CPU_ID, (void*)kernelStackTop);
    
    // Enable the syscall functionality
    enableSyscallInterface();

    // Immediately update the kernel physical base
    __kern_phys_base = reinterpret_cast<uint64_t>(params->kernelElfSegments[0].physicalBase);

    // Initialize serial ports (for headless output)
    initializeSerialPort(SERIAL_PORT_BASE_COM1);
    initializeSerialPort(SERIAL_PORT_BASE_COM2);
    initializeSerialPort(SERIAL_PORT_BASE_COM3);
    initializeSerialPort(SERIAL_PORT_BASE_COM4);
    
    // Initialize the default root kernel swapper task (this thread).
    g_kernelSwapperTasks[BSP_CPU_ID].state = ProcessState::RUNNING;
    g_kernelSwapperTasks[BSP_CPU_ID].pid = 1;
    zeromem(&g_kernelSwapperTasks[BSP_CPU_ID].context, sizeof(CpuContext));
    g_kernelSwapperTasks[BSP_CPU_ID].context.rflags |= 0x200;
    
    // Elevated flag must be 0 since we are going to lower ourselves in the next few calls.
    // TO-DO: investigate further why setting elevated flag to 1 here causes a crash. 
    g_kernelSwapperTasks[BSP_CPU_ID].elevated = 0;

    // Set the current task in the per cpu region
    __per_cpu_data.__cpu[BSP_CPU_ID].currentTask = &g_kernelSwapperTasks[BSP_CPU_ID];
    __per_cpu_data.__cpu[BSP_CPU_ID].currentTask->cpu = BSP_CPU_ID;

    __call_lowered_entry(_kuser_entry, __usermodeKernelEntryStack + USERMODE_KERNEL_ENTRY_STACK_SIZE);
}

void _kuser_entry() {
    setupInterruptDescriptorTable();

    RUN_ELEVATED({
        loadIdtr();
        enableInterrupts();
    });

    // Setup page frame allocator and lock pages with used resources
    paging::PageFrameAllocator& globalPageFrameAllocator = paging::getGlobalPageFrameAllocator();
    
    RUN_ELEVATED({
        // Initialize the global page frame allocator
        globalPageFrameAllocator.initializeFromMemoryMap(
            g_kernelEntryParameters.efiMemoryMap.base,
            g_kernelEntryParameters.efiMemoryMap.descriptorSize,
            g_kernelEntryParameters.efiMemoryMap.descriptorCount
        );

        // Update the root pml4 page table
        paging::g_kernelRootPageTable = paging::getCurrentTopLevelPageTable();
    });

    globalPageFrameAllocator.lockPage(&g_kernelEntryParameters);
    globalPageFrameAllocator.lockPages(&__ksymstart, (&__ksymend - &__ksymstart) / PAGE_SIZE + 1);
    globalPageFrameAllocator.lockPage(g_kernelEntryParameters.textRenderingFont);
    globalPageFrameAllocator.lockPages(g_kernelEntryParameters.kernelElfSegments, (&__ksymend - &__ksymstart) / PAGE_SIZE + 1);
    globalPageFrameAllocator.lockPages(
        g_kernelEntryParameters.graphicsFramebuffer.base,
        g_kernelEntryParameters.graphicsFramebuffer.size / PAGE_SIZE + 1
    );

    RUN_ELEVATED({
        // Setup the Page Attribute Table (if supported)
        if (cpuid_isPATSupported()) {
            ksetupPatOnKernelEntry();
        }

        // Initialize display and graphics context
        Display::initialize(&g_kernelEntryParameters.graphicsFramebuffer, g_kernelEntryParameters.textRenderingFont);

        char vendorName[13];
        cpuid_readVendorId(vendorName);
        kprintInfo("===== Stellux Kernel =====\n");
        kprintInfo("CPU Vendor: %s\n", vendorName);
        kprintWarn("5-level paging support: %s\n\n", cpuid_isLa57Supported() ? "enabled" : "disabled");
        debugPat(readPatMsr());
    });

    kuPrint("System total memory : %llu MB\n", globalPageFrameAllocator.getTotalSystemMemory() / 1024 / 1024);
    kuPrint("System free memory  : %llu MB\n", globalPageFrameAllocator.getFreeSystemMemory() / 1024 / 1024);
    kuPrint("System used memory  : %llu MB\n", globalPageFrameAllocator.getUsedSystemMemory() / 1024 / 1024);

    kuPrint("The kernel is loaded at:\n");
    kuPrint("    Physical : 0x%llx\n", (uint64_t)__kern_phys_base);
    kuPrint("    Virtual  : 0x%llx\n\n", (uint64_t)&__ksymstart);
    kuPrint("KernelStack  : 0x%llx\n\n", (uint64_t)g_kernelEntryParameters.kernelStack + PAGE_SIZE);

    Apic::initializeLocalApic();

    auto& acpiController = AcpiController::get();

    RUN_ELEVATED({
        acpiController.init(g_kernelEntryParameters.rsdp);
    });

    // Initialize high precision event timer and query hardware frequency
    KernelTimer::init();

    // Calibrate apic timer tickrate to 100 milliseconds
    KernelTimer::calibrateApicTimer(100);

    // Start the kernel-wide APIC periodic timer
    KernelTimer::startApicPeriodicTimer();

    // Initialize IOAPIC
    if (acpiController.hasApicTable()) {
        kstl::SharedPtr<IoApic>& ioapic = acpiController.getApicTable()->getIoApic(0);

        // Enable keyboard IRQ
        IoApic::RedirectionEntry entry;
        memset(&entry, 0, sizeof(IoApic::RedirectionEntry));

        uint8_t ioapicEntryNo = 1;
        entry.vector = IRQ1;
        entry.destination = BSP_CPU_ID;
        ioapic->writeRedirectionEntry(ioapicEntryNo, &entry);
    }

    // Initialize the scheduler
    auto& sched = RRScheduler::get();
    sched.init();

    // Bring up all available processor cores
    initializeApCores();

#ifdef KE_TEST_MULTITHREADING
    ke_test_multithreading();
#endif

#ifdef KE_TEST_XHCI_INIT
    ke_test_xhci_init();
#endif

#ifdef KE_TEST_AP_STARTUP
    ke_test_ap_startup();
#endif

#ifdef KE_TEST_CPU_TEMP_READINGS
    ke_test_read_cpu_temps();
#endif

#ifdef KE_TEST_PRINT_CURRENT_TIME
    ke_test_print_current_time();
#endif

#ifdef KE_TEST_GRAPHICS
    ke_test_graphics();
#endif

    // Infinite loop
    while (1) { __asm__ volatile("nop"); }
}
