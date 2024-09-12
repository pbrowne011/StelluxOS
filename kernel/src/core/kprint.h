#ifndef KPRINT_H
#define KPRINT_H
#include <core/ktypes.h>

#define TEXT_COLOR_WHITE    0xffffffff
#define TEXT_COLOR_BLACK    0xff000000
#define TEXT_COLOR_RED      0xffff0000
#define TEXT_COLOR_GREEN    0xff00ff00
#define TEXT_COLOR_BLUE     0xff0000ff
#define TEXT_COLOR_YELLOW   0xffffff00
#define TEXT_COLOR_COOL     0xff05ffa4

#define DEFAULT_TEXT_COLOR  TEXT_COLOR_COOL

#define WARN "[WARN] "
#define ERROR "[ERROR] "
#define INFO "[INFO] "
#define KLOGSIZE_PAGES 2

__PRIVILEGED_CODE
void kprintSetCursorLocation(uint32_t x, uint32_t y);

__PRIVILEGED_CODE
void kprintCharColored(
    char chr,
	unsigned int color
);

__PRIVILEGED_CODE
void kprintChar(
    char chr
);

__PRIVILEGED_CODE
void kprintColoredEx(
    const char* str,
    uint32_t color
);

__PRIVILEGED_CODE
void kprintFmtColored(
    uint32_t color,
    const char* fmt,
    ...
);

__PRIVILEGED_CODE
void kprintFmtColoredLocked(
    uint32_t color,
    const char* fmt,
    ...
);

__PRIVILEGED_CODE
void kprint(
    const char* fmt,
    ...
);

__PRIVILEGED_CODE
void kprintLocked(
    const char* fmt,
    ...
);

void kuPrint(
    const char* fmt,
    ...
);

__PRIVILEGED_CODE
void kdmesg(
    char* output,
    size_t maxLength
);
__PRIVILEGED_CODE
void kprintKernelLog();

#endif
