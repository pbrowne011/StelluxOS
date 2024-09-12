#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <core/ktypes.h>
#include <memory/kmemory.h>
#include <sync.h>

class CircularBuffer {
private:
    char* buffer;
    size_t size;
    size_t head;
    size_t tail;
    bool full;
    SpinLock lock;

public:
    CircularBuffer(size_t bufferSize);
    ~CircularBuffer();

    void write(const char* data, size_t length);
    size_t read(char* output, size_t maxLength);
    bool isEmpty() const;
    bool isFull() const;
};

#endif // CIRCULAR_BUFFER_H
