#pragma once

#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "transport.hpp"
#include "protocol.hpp"

// --------------------------------------------------
// RX → parser chunk
// --------------------------------------------------

struct Chunk
{
    std::vector<uint8_t> bytes;
};

// --------------------------------------------------
// Application packet handler
// --------------------------------------------------

class AppPacketHandler : public PacketHandler
{
public:
    void on_finger_packet(
        uint16_t seq,
        uint64_t timestamp,
        const FingerData* fingers,
        uint8_t count) override;

    void on_unknown(
        uint8_t type,
        uint16_t seq,
        const uint8_t* payload,
        uint16_t len) override;
};

// --------------------------------------------------
// Runtime container
// --------------------------------------------------

struct Runtime
{
    std::unique_ptr<ITransport> transport;

    std::atomic<bool> running{true};

    // RX → parser queue
    std::mutex m;
    std::condition_variable cv;
    std::deque<Chunk> queue;

    // handler
    AppPacketHandler handler;
};

// --------------------------------------------------
// Thread entry points
// --------------------------------------------------

void rx_thread_fn(Runtime& rt);
void parser_thread_fn(Runtime& rt);
void tx_thread_fn(Runtime& rt);