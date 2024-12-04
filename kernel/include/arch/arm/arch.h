#ifndef ARM_ARCH_H
#define ARM_ARCH_H

#include <types.h>

namespace arm {
    void init_exception_level();
    void init_vectors();
    void enable_irq();
}

// Non-namespaced functions
inline void enable_interrupts() {
    arm::enable_irq();
}

void enable_syscall_interface();

#endif // ARM_ARCH_H