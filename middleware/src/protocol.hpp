#pragma once
#include <cstdint>
#include <vector>
#include "ring_buffer.hpp"

// ---------------- constants ----------------

constexpr uint16_t MAGIC = 0xA55A;

#pragma pack(push,1)
struct PacketHeader {
    uint16_t magic;
    uint16_t size;
    uint16_t seq;
    uint8_t  type;
};

struct FingerData {
    double x;
    double y;
    double z;
    uint32_t state_array;
    float temp;
};
#pragma pack(pop)

// ---------------- callbacks ----------------

struct PacketHandler {
    virtual void on_finger_packet(
        uint16_t seq,
        uint64_t timestamp,
        const FingerData* fingers,
        uint8_t count) = 0;

    virtual void on_unknown(
        uint8_t type,
        uint16_t seq,
        const uint8_t* payload,
        uint16_t len) = 0;

    virtual ~PacketHandler() = default;
};

// ---------------- API ----------------

uint32_t crc32(const uint8_t* data, size_t len);

void parse_from_ring(
    ByteRing& ring,
    PacketHandler& handler);

std::vector<uint8_t> build_heartbeat(uint16_t seq);