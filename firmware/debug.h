// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "engine.h"

namespace atmosphere {
inline constexpr const char* kVersion = "0.3.0";
inline constexpr const char* kControlNames[] = {
    "mix", "time", "wet_modify", "clock", "length", "loop_modify",
    "wet_mode", "routing", "loop_mode", "fs1", "fs2",
    "wet_on", "loop_on", "freeze", "overdub"
};
constexpr unsigned kPanelControls = 11, kControls = 15;
enum class Operation { Set, Clear, Get, Watch, Ping, Dfu };
struct Command { Operation operation = Operation::Get; unsigned field = kControls, value = 0, id = 0; };
// Knob wire values: 0..10000 = 0..100%; toggle values: 0/1/2 = up/middle/down.
struct Snapshot {
    unsigned physical[kPanelControls] = {};
    unsigned effective[kPanelControls] = {};
    unsigned requested[kControls] = {};
    unsigned mask = 0, state = 0, divider = 1, peak = 0, applied = 0;
};
struct Panel {
    float knobs[6] = {};
    unsigned toggles[3] = {};
    bool feet[2] = {};
};

template <typename T, unsigned N> class SpscQueue {
  public:
    bool Push(const T& item) {
        const unsigned h = head_.load(std::memory_order_relaxed);
        const unsigned next = (h + 1) % N;
        if(next == tail_.load(std::memory_order_acquire)) return false;
        items_[h] = item; head_.store(next, std::memory_order_release); return true;
    }
    bool Pop(T& item) {
        const unsigned t = tail_.load(std::memory_order_relaxed);
        if(t == head_.load(std::memory_order_acquire)) return false;
        item = items_[t]; tail_.store((t + 1) % N, std::memory_order_release); return true;
    }
  private:
    T items_[N] = {};
    std::atomic<unsigned> head_{0}, tail_{0};
};

// Atomic storage makes the versioned snapshot legal in the C++ memory model.
// Audio never waits for the reader. Main may retry if interrupted by audio.
class SnapshotMailbox {
  public:
    void Publish(const Snapshot& s);
    Snapshot Read() const;
  private:
    static constexpr unsigned kWords = kPanelControls * 2 + kControls + 5;
    std::atomic<unsigned> version_{0};
    std::atomic<unsigned> words_[kWords]{};
};

class DebugControls {
  public:
    // A physical move releases only that control's stored USB value. Return
    // button-source changes that need gesture resynchronization.
    unsigned Observe(const Panel& physical);
    void Apply(const Command& command);
    Panel Resolve(const Panel& physical) const;
    bool Overridden(unsigned field) const { return (mask_ & (1u << field)) != 0; }
    bool Boolean(unsigned field, bool normal) const {
        return mask_ & (1u << field) ? values_[field] != 0 : normal;
    }
    void Fill(Snapshot& s, const Panel& physical, const Panel& effective) const;
  private:
    unsigned values_[kControls] = {}, mask_ = 0, applied_ = 0;
    Panel physical_{};
    float knob_anchor_[6] = {};
    unsigned toggle_anchor_[3] = {};
    bool observed_ = false;
};

bool ParseCommand(const char* line, Command& result, const char*& error);
size_t FormatSnapshot(char* buffer, size_t capacity, const Snapshot& s, unsigned request = 0);

// USB ISR enqueues bytes only. Overlong lines and overflow are discarded up
// to the next newline so a command suffix can never be executed on its own.
class CommandReceiver {
  public:
    void Receive(const uint8_t* bytes, size_t count);
    enum class Result { None, Line, TooLong, Overflow };
    Result Next(char* line, size_t capacity);
  private:
    SpscQueue<uint16_t, 2048> bytes_;
    bool dropping_ = false;
    char line_[128] = {};
    size_t used_ = 0;
    bool too_long_ = false;
};
} // namespace atmosphere
