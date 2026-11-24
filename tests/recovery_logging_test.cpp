// SPDX-License-Identifier: GPL-3.0-or-later
#include "usb_output.h"
#include "hid/usb.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdexcept>
static unsigned attempts = 0;
namespace daisy {
void UsbHandle::Init(UsbPeriph) {}
UsbHandle::Result UsbHandle::TransmitInternal(uint8_t*, size_t) {
    if(++attempts > 64) throw std::runtime_error("logger keeps retrying busy USB and blocks recovery");
    return attempts <= 2 ? Result::OK : Result::ERR;
}
UsbHandle::Result UsbHandle::TransmitExternal(uint8_t*, size_t) { return Result::ERR; }
}
int main() {
    try {
        atmosphere::UsbOutput log;
        daisy::UsbHandle usb;
        auto send = [&](uint8_t* data, size_t bytes) {
            return usb.TransmitInternal(data, bytes) == daisy::UsbHandle::Result::OK;
        };
        for(unsigned i = 0; i < 10; ++i) {
            log.Push("status\r\n", 8);
            log.Poll(send);
        }
        assert(attempts == 10); // One attempt per report, even after two successes.
        // Verify busy USB keeps ownership of an unchanged in-flight buffer.
        atmosphere::UsbOutput lifetime;
        uint8_t* pending = nullptr;
        char snapshot[128] = {};
        lifetime.Push("first\r\n", 8);
        lifetime.Poll([&](uint8_t* data, size_t bytes) {
            pending = data;
            std::memcpy(snapshot, data, bytes);
            return true;
        });
        for(unsigned i = 0; i < 10; ++i) {
            lifetime.Push("later\r\n", 8);
            lifetime.Poll([](uint8_t*, size_t) { return false; });
            assert(std::strcmp(reinterpret_cast<char*>(pending), snapshot) == 0);
        }
        std::puts("PASS: main loop can service boot request with USB busy");
        return 0;
    } catch(const std::exception& e) {
        std::printf("FAIL: %s (%u attempts)\n", e.what(), attempts);
        return 1;
    }
}
