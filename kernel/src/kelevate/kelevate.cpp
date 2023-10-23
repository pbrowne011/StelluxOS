#include "kelevate.h"
#include <syscall/syscalls.h>

EXTERN_C void __call_lowered_entry_asm(void* entry, void* stack, uint64_t flags);

void __kelevate() {
    do_syscall_64(SYSCALL_SYS_ELEVATE, 0, 0, 0, 0, 0, 0);
}

void __klower() {
    do_syscall_64(SYSCALL_SYS_LOWER, 0, 0, 0, 0, 0, 0);
}

void __call_lowered_entry(lowered_entry_fn_t entry, void* user_stack) {
    __call_lowered_entry_asm((void*)entry, (void*)((uint64_t)user_stack + 0x1000), 0x200);
}
