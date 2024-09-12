#include "kprint.h"
#include "kstring.h"
#include "kbuff.h"
#include "circular_buffer.h"
#include <graphics/kdisplay.h>
#include <ports/serial.h>
#include <stdarg.h>
#include <sync.h>
#include <kelevate/kelevate.h>

#define CHAR_PIXEL_WIDTH 8
#define CHAR_TOP_BORDER_OFFSET 8
#define CHAR_LEFT_BORDER_OFFSET 8
#define KERNEL_LOG_BUFFER_SIZE (KLOGSIZE_PAGES * PAGE_SIZE)

__PRIVILEGED_DATA
Point g_cursorLocation = { .x = CHAR_LEFT_BORDER_OFFSET, .y = CHAR_TOP_BORDER_OFFSET };

__PRIVILEGED_DATA
CircularBuffer g_kernelLogBuffer(KERNEL_LOG_BUFFER_SIZE);

DECLARE_SPINLOCK(__kprint_spinlock);

__PRIVILEGED_CODE
void kprintSetCursorLocation(uint32_t x, uint32_t y) {
    g_cursorLocation.x = (x == static_cast<uint32_t>(-1)) ? CHAR_LEFT_BORDER_OFFSET : x;
    g_cursorLocation.y = (y == static_cast<uint32_t>(-1)) ? CHAR_TOP_BORDER_OFFSET : y;
}

__PRIVILEGED_CODE
void kprintCharColored(char chr, unsigned int color) {
    // Log the character to the serial port
    writeToSerialPort(SERIAL_PORT_BASE_COM1, chr);

    // Log the character to the circular buffer
    g_kernelLogBuffer.write(&chr, 1);

    // Render the character on screen (commented out for brevity)
    /*
    uint8_t charPixelHeight = Display::getTextFontInfo()->header->charSize;
    switch (chr) {
        case '\n':
            // Handle newline
            break;
        case '\r':
            // Handle carriage return
            break;
        default:
            // Render character
            break;
    }
    */
}

__PRIVILEGED_CODE
void kprintChar(char chr) {
    kprintCharColored(chr, DEFAULT_TEXT_COLOR);
}

__PRIVILEGED_CODE
void kprintColoredEx(const char* str, uint32_t color) {
    char* chr = const_cast<char*>(str);
    while (*chr) {
        kprintCharColored(*chr, color);
        ++chr;
    }
}

__PRIVILEGED_CODE
void kprintFmtColoredEx(uint32_t color, const char* fmt, va_list args) {
    char buffer[1024]; // Temporary buffer for formatted string
    int length = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (length > 0) {
        // Write to circular buffer
        g_kernelLogBuffer.write(buffer, length);

        // Write to serial port and screen
        for (int i = 0; i < length; ++i) {
            kprintCharColored(buffer[i], color);
        }
    }

    Display::swapBuffers();
}

void kprintFmtColoredExLocked(uint32_t color, const char* fmt, va_list args) {
    acquireSpinlock(&__kprint_spinlock);
    kprintFmtColoredEx(color, fmt, args);
    releaseSpinlock(&__kprint_spinlock);
}

__PRIVILEGED_CODE
void kprintFmtColored(uint32_t color, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kprintFmtColoredEx(color, fmt, args);
    va_end(args);
}

__PRIVILEGED_CODE
void kprint(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kprintFmtColoredEx(DEFAULT_TEXT_COLOR, fmt, args);
    va_end(args);
}

__PRIVILEGED_CODE
void kprintLocked(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    kprintFmtColoredExLocked(DEFAULT_TEXT_COLOR, fmt, args);
    va_end(args);
}

void kuPrint(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    RUN_ELEVATED({
        kprintFmtColoredExLocked(DEFAULT_TEXT_COLOR, fmt, args);
    });
    va_end(args);
}

__PRIVILEGED_CODE
void kdmesg(char* output, size_t maxLength) {
    size_t bytesRead = g_kernelLogBuffer.read(output, maxLength);
    output[bytesRead] = '\0'; // Null-terminate the output
}

__PRIVILEGED_CODE
void kprintKernelLog() {
    char buffer[1024];
    size_t bytesRead;
    
    while ((bytesRead = g_kernelLogBuffer.read(buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        kprintColoredEx(buffer, DEFAULT_TEXT_COLOR);
    }
}
