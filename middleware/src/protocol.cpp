#include "protocol.hpp"
#include <cstring>

// ---------------- CRC ----------------

uint32_t crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int)(crc & 1u));
    }

    return ~crc;
}

// ---------------- parser ----------------

void parse_from_ring(ByteRing& ring, PacketHandler& handler)
{
    uint8_t header_buf[sizeof(PacketHeader)];

    while (true)
    {
        if (ring.size() < sizeof(PacketHeader) + 4)
            return;

        if (!ring.peek(header_buf, sizeof(PacketHeader)))
            return;

        PacketHeader* hdr =
            reinterpret_cast<PacketHeader*>(header_buf);

        if (hdr->magic != MAGIC)
        {
            ring.consume(1);
            continue;
        }

        size_t total =
            sizeof(PacketHeader) + hdr->size + 4;

        if (ring.size() < total)
            return;

        std::vector<uint8_t> pkt(total);
        ring.read(pkt.data(), total);

        uint32_t crc_expected =
            *reinterpret_cast<uint32_t*>(
                pkt.data() + total - 4);

        uint32_t crc_actual =
            crc32(pkt.data(), total - 4);

        if (crc_expected != crc_actual)
            continue;

        const uint8_t* payload =
            pkt.data() + sizeof(PacketHeader);

        if (hdr->type == 1)
        {
            if (hdr->size < 9) continue;

            uint64_t ts =
                *reinterpret_cast<const uint64_t*>(payload);

            uint8_t count = payload[8];

            size_t need = 9 + count * sizeof(FingerData);
            if (hdr->size < need) continue;

            const FingerData* fingers =
                reinterpret_cast<const FingerData*>(payload + 9);

            handler.on_finger_packet(
                hdr->seq,
                ts,
                fingers,
                count);
        }
        else
        {
            handler.on_unknown(
                hdr->type,
                hdr->seq,
                payload,
                hdr->size);
        }
    }
}

// ---------------- builder ----------------

std::vector<uint8_t> build_heartbeat(uint16_t seq)
{
    const uint8_t payload[1] = {1};

    PacketHeader hdr{};
    hdr.magic = MAGIC;
    hdr.size  = sizeof(payload);
    hdr.seq   = seq;
    hdr.type  = 2;

    size_t total =
        sizeof(hdr) + sizeof(payload) + 4;

    std::vector<uint8_t> buf(total);

    size_t off = 0;

    memcpy(buf.data()+off, &hdr, sizeof(hdr));
    off += sizeof(hdr);

    memcpy(buf.data()+off, payload, sizeof(payload));
    off += sizeof(payload);

    uint32_t crc = crc32(buf.data(), off);
    memcpy(buf.data()+off, &crc, 4);

    return buf;
}