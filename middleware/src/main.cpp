#include "main.hpp"
#include "ring_buffer.hpp"

#include <thread>
#include <chrono>
#include <cstdio>

// --------------------------------------------------
// Packet handler implementation
// --------------------------------------------------

void AppPacketHandler::on_finger_packet(
    uint16_t seq,
    uint64_t timestamp,
    const FingerData* fingers,
    uint8_t count)
{
    std::printf("[FINGERS] seq=%u ts=%llu count=%u\n",
        seq,
        (unsigned long long)timestamp,
        (unsigned)count);

    // Uncomment for per-finger debug
    /*
    for (uint8_t i = 0; i < count; ++i)
    {
        const auto& f = fingers[i];
        printf("  i=%u pos=(%.3f %.3f %.3f) state=0x%08X temp=%.2f\n",
            i, f.x, f.y, f.z, f.state_array, f.temp);
    }
    */
}

void AppPacketHandler::on_unknown(
    uint8_t type,
    uint16_t seq,
    const uint8_t*,
    uint16_t len)
{
    std::printf("[UNKNOWN] type=%u seq=%u len=%u\n",
        type, seq, len);
}

// --------------------------------------------------
// RX thread
// --------------------------------------------------

void rx_thread_fn(Runtime& rt)
{
    uint8_t buf[1024];

    while (rt.running.load())
    {
        int n = rt.transport->read(buf, sizeof(buf));

        if (n <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        Chunk c;
        c.bytes.assign(buf, buf + n);

        {
            std::lock_guard<std::mutex> lk(rt.m);
            rt.queue.push_back(std::move(c));
        }

        rt.cv.notify_one();
    }
}

// --------------------------------------------------
// Parser thread
// --------------------------------------------------

void parser_thread_fn(Runtime& rt)
{
    ByteRing ring(8192);

    while (rt.running.load())
    {
        Chunk c;

        {
            std::unique_lock<std::mutex> lk(rt.m);
            rt.cv.wait(lk, [&]{
                return !rt.queue.empty() || !rt.running.load();
            });

            if (!rt.running.load())
                break;

            c = std::move(rt.queue.front());
            rt.queue.pop_front();
        }

        ring.push(c.bytes.data(), c.bytes.size());
        parse_from_ring(ring, rt.handler);
    }
}

// --------------------------------------------------
// TX thread (heartbeat)
// --------------------------------------------------

void tx_thread_fn(Runtime& rt)
{
    uint16_t seq = 0;

    while (rt.running.load())
    {
        auto pkt = build_heartbeat(seq++);
        rt.transport->write(pkt.data(), pkt.size());

        std::this_thread::sleep_for(
            std::chrono::milliseconds(10)); // 100 Hz
    }
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------

int main()
{
    Runtime rt;

#ifdef USE_SIM
    std::printf("Running SIM transport\n");
    rt.transport = std::make_unique<SimTransport>();
#elif defined(USE_USB)
    std::printf("Running USB transport\n");
    rt.transport = std::make_unique<USBTransport>();
#else
    std::printf("No transport defined\n");
    return 1;
#endif

    if (!rt.transport->open())
    {
        std::printf("Transport open failed\n");
        return 1;
    }

    std::thread rx(rx_thread_fn, std::ref(rt));
    std::thread parser(parser_thread_fn, std::ref(rt));
    std::thread tx(tx_thread_fn, std::ref(rt));

    // demo loop
    for (;;)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    // cleanup
    rt.running.store(false);
    rt.cv.notify_all();

    rx.join();
    parser.join();
    tx.join();

    rt.transport->close();
    return 0;
}