#include <types.h>
#include <memory/memory.h>
#include <serial/serial.h>
#include <arch/percpu.h>
#include <arch/arm/arch.h>
#include <syscall/syscalls.h>
#include <sched/sched.h>
#include <dynpriv/dynpriv.h>

// BSP system stack - same concept as x86
uint8_t g_default_bsp_system_stack[0x2000];

namespace arch {

__PRIVILEGED_CODE
void arch_init() {
    // Setup kernel stack - similar to x86
    uint64_t bsp_system_stack_top = reinterpret_cast<uint64_t>(g_default_bsp_system_stack) +
                                   sizeof(g_default_bsp_system_stack) - 0x10;

    // Instead of GDT (x86 specific), setup ARM exception levels and system registers
    // This will be implemented in ARM's exception level handling code
    arm::init_exception_level();
    
    // Instead of IDT, setup ARM exception vector table
    arm::init_vectors();
    enable_interrupts();  // ARM equivalent: enable_irq()

    // Setup per-CPU area for BSP - ARM version
    init_bsp_per_cpu_area();

    // Setup BSP's idle task (similar to x86)
    task_control_block* bsp_idle_task = sched::get_idle_task(BSP_CPU_ID);
    zeromem(bsp_idle_task, sizeof(task_control_block));
    this_cpu_write(current_task, bsp_idle_task);
    current->system_stack = bsp_system_stack_top;
    current->cpu = 0;
    current->elevated = 1;
    current->state = process_state::RUNNING;
    current->pid = 0;

    // Enable syscall interface - ARM uses SVC instruction instead of SYSCALL
    enable_syscall_interface();

    // Setup dynamic privilege (same concept, different implementation)
    dynpriv::use_current_asid();
}

} // namespace arch