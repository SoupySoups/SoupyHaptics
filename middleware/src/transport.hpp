#pragma once
#include <cstdint>

class ITransport {
public:
    virtual bool open() = 0;
    virtual int  read(uint8_t* buf, int len) = 0;
    virtual int  write(const uint8_t* buf, int len) = 0;
    virtual void close() = 0;
    virtual ~ITransport() {}
};

#ifdef USE_USB
class USBTransport : public ITransport {
public:
    bool open() override;
    int  read(uint8_t*, int) override;
    int  write(const uint8_t*, int) override;
    void close() override;
};
#endif

#ifdef USE_SIM
class SimTransport : public ITransport {
public:
    bool open() override;
    int  read(uint8_t*, int) override;
    int  write(const uint8_t*, int) override;
    void close() override;
};
#endif