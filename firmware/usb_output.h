// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace atmosphere {
// Main-loop-only bounded output. Neither a disconnected reader nor a busy
// endpoint can block audio/recovery. Flight buffers retain USB ownership.
class UsbOutput {
  public:
    static constexpr size_t kBytes = 1536;
    bool Push(const char* line, size_t bytes) {
        if(!bytes || bytes >= kBytes || used_ == 8) return false;
        auto& slot = queued_[(head_ + used_) % 8];
        std::memcpy(slot.data, line, bytes); slot.bytes = bytes; ++used_; return true;
    }
    unsigned Free() const { return 8 - used_; }
    template <typename Transmit> void Poll(Transmit transmit) {
        if(!used_) return;
        auto& item = queued_[head_];
        std::memcpy(flight_[next_], item.data, item.bytes);
        if(!transmit(reinterpret_cast<uint8_t*>(flight_[next_]), item.bytes)) return;
        next_ ^= 1; head_ = (head_ + 1) % 8; --used_;
    }
  private:
    struct Item { char data[kBytes]; size_t bytes = 0; } queued_[8];
    char flight_[2][kBytes] = {};
    unsigned head_ = 0, used_ = 0, next_ = 0;
};
} // namespace atmosphere
