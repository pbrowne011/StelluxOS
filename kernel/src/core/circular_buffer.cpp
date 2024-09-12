#include "circular_buffer.h"

CircularBuffer::CircularBuffer(size_t bufferSize) 
    : size(bufferSize), head(0), tail(0), full(false) {
    buffer = static_cast<char*>(zallocPages((bufferSize + PAGE_SIZE - 1) / PAGE_SIZE));
    initializeSpinLock(&lock);
}

CircularBuffer::~CircularBuffer() {
    freePages(buffer);
}

void CircularBuffer::write(const char* data, size_t length) {
    acquireSpinlock(&lock);

    for (size_t i = 0; i < length; ++i) {
        buffer[head] = data[i];
        head = (head + 1) % size;

        if (full) {
            tail = (tail + 1) % size;
        }

        full = head == tail;
    }

    releaseSpinlock(&lock);
}

size_t CircularBuffer::read(char* output, size_t maxLength) {
    acquireSpinlock(&lock);

    size_t count = 0;
    while (count < maxLength && (full || head != tail)) {
        output[count] = buffer[tail];
        tail = (tail + 1) % size;
        full = false;
        ++count;
    }

    releaseSpinlock(&lock);
    return count;
}

bool CircularBuffer::isEmpty() const {
    return (!full && (head == tail));
}

bool CircularBuffer::isFull() const {
    return full;
}
