#ifndef BOOT_INIT_H
#define BOOT_INIT_H

#include <types.h>

namespace boot {
    void walk_bootinfo(void* info);
    void handle_invalid_bootmagic();
}

#endif