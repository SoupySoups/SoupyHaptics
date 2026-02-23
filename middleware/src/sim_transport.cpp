#include "transport.hpp"
#include "protocol.hpp"

#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <chrono>
#include <random>

class SimTransportImpl {
    public:
        std::thread worker;
        std::atomic<bool> running{false};

        std::mutex m;
        std::vector<uint8_t> buffer;

        uint16_t seq = 0;

        void generate_loop()
        {
            std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<double> dist(-1.0, 1.0);
            std::uniform_real_distribution<float>  temp(20.f, 40.f);

            while (running.load())
            {
                // Build fake finger packet
                uint64_t ts =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count();

                uint8_t count = 5;

                std::vector<FingerData> fingers(count);

                for (uint8_t i = 0; i < count; ++i)
                {
                    fingers[i].x = dist(rng);
                    fingers[i].y = dist(rng);
                    fingers[i].z = dist(rng);
                    fingers[i].state_array = i;
                    fingers[i].temp = temp(rng);
                }

                // Build payload
                std::vector<uint8_t> payload;

                payload.resize(9 + count*sizeof(FingerData));

                std::memcpy(payload.data(), &ts, 8);
                payload[8] = count;

                std::memcpy(
                    payload.data() + 9,
                    fingers.data(),
                    count*sizeof(FingerData));

                PacketHeader hdr{};
                hdr.magic = MAGIC;
                hdr.size  = payload.size();
                hdr.seq   = seq++;
                hdr.type  = 1;

                size_t total =
                    sizeof(hdr) + payload.size() + 4;

                std::vector<uint8_t> pkt(total);

                size_t off = 0;

                std::memcpy(pkt.data()+off, &hdr, sizeof(hdr));
                off += sizeof(hdr);

                std::memcpy(pkt.data()+off, payload.data(), payload.size());
                off += payload.size();

                uint32_t crc = crc32(pkt.data(), off);
                std::memcpy(pkt.data()+off, &crc, 4);

                {
                    std::lock_guard<std::mutex> lk(m);
                    buffer.insert(buffer.end(), pkt.begin(), pkt.end());
                }
            }
        }
};

static SimTransportImpl g_sim;

bool SimTransport::open()
{
    g_sim.running.store(true);
    g_sim.worker = std::thread(&SimTransportImpl::generate_loop, &g_sim);
    return true;
}

int SimTransport::read(uint8_t* out, int maxlen)
{
    std::lock_guard<std::mutex> lk(g_sim.m);

    if (g_sim.buffer.empty())
        return 0;

    int n = std::min<int>(maxlen, g_sim.buffer.size());

    std::memcpy(out, g_sim.buffer.data(), n);

    g_sim.buffer.erase(
        g_sim.buffer.begin(),
        g_sim.buffer.begin() + n);

    return n;
}

int SimTransport::write(const uint8_t*, int len)
{
    // ignore writes in sim
    return len;
}

void SimTransport::close()
{
    g_sim.running.store(false);

    if (g_sim.worker.joinable())
        g_sim.worker.join();
}