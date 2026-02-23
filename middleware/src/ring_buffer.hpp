#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <algorithm>

class ByteRing
{
public:
    explicit ByteRing(size_t capacity)
        : buf(capacity), head(0), tail(0), full(false) {}

    size_t capacity() const { return buf.size(); }

    size_t size() const
    {
        if (full) return buf.size();
        if (head >= tail) return head - tail;
        return buf.size() - tail + head;
    }

    size_t free_space() const
    {
        return buf.size() - size();
    }

    bool empty() const
    {
        return (!full && head == tail);
    }

    void push(const uint8_t* data, size_t len)
    {
        for (size_t i = 0; i < len; ++i)
        {
            buf[head] = data[i];
            head = (head + 1) % buf.size();

            if (full)
                tail = (tail + 1) % buf.size();

            full = (head == tail);
        }
    }

    bool peek(uint8_t* out, size_t len) const
    {
        if (size() < len) return false;

        size_t idx = tail;
        for (size_t i = 0; i < len; ++i)
        {
            out[i] = buf[idx];
            idx = (idx + 1) % buf.size();
        }
        return true;
    }

    bool read(uint8_t* out, size_t len)
    {
        if (!peek(out, len)) return false;
        consume(len);
        return true;
    }

    void consume(size_t len)
    {
        tail = (tail + len) % buf.size();
        full = false;
    }

private:
    std::vector<uint8_t> buf;
    size_t head;
    size_t tail;
    bool full;
};