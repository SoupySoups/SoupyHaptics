#include "transport.hpp"

#include <libusb.h>
#include <cstdio>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstring>

static const uint16_t VID = 0x1d50;
static const uint16_t PID = 0xdead;

static const uint8_t EP_IN  = 0x81;
static const uint8_t EP_OUT = 0x01;

static const int IFACE = 0;

// tune these
static const int NUM_IN_TRANSFERS = 8;
static const int IN_XFER_SIZE     = 1024;

struct InXferCtx {
    libusb_transfer* xfer = nullptr;
    uint8_t* buf = nullptr;
};

class USBTransportImpl {
public:
    libusb_context* ctx = nullptr;
    libusb_device_handle* handle = nullptr;

    std::thread event_thread;
    std::atomic<bool> running{false};

    std::mutex rx_m;
    std::deque<std::vector<uint8_t>> rx_q;

    InXferCtx in_ctx[NUM_IN_TRANSFERS]{};

    static void LIBUSB_CALL on_in_transfer(libusb_transfer* t)
    {
        auto* self = reinterpret_cast<USBTransportImpl*>(t->user_data);
        if (!self || !self->running.load()) return;

        if (t->status == LIBUSB_TRANSFER_COMPLETED && t->actual_length > 0) {
            std::vector<uint8_t> chunk((size_t)t->actual_length);
            std::memcpy(chunk.data(), t->buffer, (size_t)t->actual_length);

            {
                std::lock_guard<std::mutex> lk(self->rx_m);
                self->rx_q.push_back(std::move(chunk));
                // optional: cap queue size to avoid runaway memory
                if (self->rx_q.size() > 256) self->rx_q.pop_front();
            }
        }

        // resubmit to keep streaming (unless shutting down)
        if (self->running.load()) {
            int r = libusb_submit_transfer(t);
            if (r != 0) {
                // If resubmission fails, stop running
                self->running.store(false);
            }
        }
    }

    bool start_async_in()
    {
        for (int i = 0; i < NUM_IN_TRANSFERS; ++i) {
            in_ctx[i].buf = (uint8_t*)std::malloc(IN_XFER_SIZE);
            if (!in_ctx[i].buf) return false;

            in_ctx[i].xfer = libusb_alloc_transfer(0);
            if (!in_ctx[i].xfer) return false;

            libusb_fill_bulk_transfer(
                in_ctx[i].xfer,
                handle,
                EP_IN,
                in_ctx[i].buf,
                IN_XFER_SIZE,
                &USBTransportImpl::on_in_transfer,
                this,
                0 // timeout: 0 = unlimited (fine for async; events thread handles it)
            );

            int r = libusb_submit_transfer(in_ctx[i].xfer);
            if (r != 0) return false;
        }
        return true;
    }

    void stop_async_in()
    {
        for (int i = 0; i < NUM_IN_TRANSFERS; ++i) {
            if (in_ctx[i].xfer) {
                libusb_cancel_transfer(in_ctx[i].xfer);
            }
        }

        // pump events a bit so cancellations complete
        for (int i = 0; i < 50; ++i) {
            timeval tv{0, 10'000}; // 10ms
            libusb_handle_events_timeout(ctx, &tv);
        }

        for (int i = 0; i < NUM_IN_TRANSFERS; ++i) {
            if (in_ctx[i].xfer) {
                libusb_free_transfer(in_ctx[i].xfer);
                in_ctx[i].xfer = nullptr;
            }
            if (in_ctx[i].buf) {
                std::free(in_ctx[i].buf);
                in_ctx[i].buf = nullptr;
            }
        }
    }

    void event_loop()
    {
        while (running.load()) {
            timeval tv{0, 50'000}; // 50ms
            libusb_handle_events_timeout(ctx, &tv);
        }

        // flush remaining events on shutdown
        for (int i = 0; i < 10; ++i) {
            timeval tv{0, 10'000};
            libusb_handle_events_timeout(ctx, &tv);
        }
    }

    bool open()
    {
        if (libusb_init(&ctx) != 0) {
            std::printf("libusb_init failed\n");
            return false;
        }

        handle = libusb_open_device_with_vid_pid(ctx, VID, PID);
        if (!handle) {
            std::printf("USB device not found (VID/PID)\n");
            return false;
        }

        // auto-detach kernel driver on Windows typically not needed, but harmless:
        libusb_set_auto_detach_kernel_driver(handle, 1);

        int r = libusb_claim_interface(handle, IFACE);
        if (r != 0) {
            std::printf("claim interface failed: %d\n", r);
            return false;
        }

        running.store(true);

        if (!start_async_in()) {
            std::printf("start_async_in failed\n");
            running.store(false);
            return false;
        }

        event_thread = std::thread([this](){ event_loop(); });

        std::printf("USB async RX started\n");
        return true;
    }

    void close()
    {
        running.store(false);

        stop_async_in();

        if (event_thread.joinable()) event_thread.join();

        if (handle) {
            libusb_release_interface(handle, IFACE);
            libusb_close(handle);
            handle = nullptr;
        }

        if (ctx) {
            libusb_exit(ctx);
            ctx = nullptr;
        }
    }

    int read(uint8_t* out, int maxlen)
    {
        std::lock_guard<std::mutex> lk(rx_m);
        if (rx_q.empty()) return 0;

        auto& front = rx_q.front();
        int n = (int)front.size();
        if (n > maxlen) n = maxlen;

        std::memcpy(out, front.data(), (size_t)n);

        // if chunk larger than maxlen, keep remainder
        if ((size_t)n < front.size()) {
            front.erase(front.begin(), front.begin() + n);
        } else {
            rx_q.pop_front();
        }

        return n;
    }

    int write(const uint8_t* data, int len)
    {
        // Keep TX simple/robust for now (sync). RX is the critical async path.
        int transferred = 0;
        int r = libusb_bulk_transfer(handle, EP_OUT,
                                     (unsigned char*)data, len,
                                     &transferred,
                                     10 /*ms timeout*/);
        if (r != 0) return 0;
        return transferred;
    }
};

// one instance owned by USBTransport wrapper
static USBTransportImpl g_usb;

bool USBTransport::open() { return g_usb.open(); }
int  USBTransport::read(uint8_t* b, int n) { return g_usb.read(b, n); }
int  USBTransport::write(const uint8_t* b, int n) { return g_usb.write(b, n); }
void USBTransport::close() { g_usb.close(); }
