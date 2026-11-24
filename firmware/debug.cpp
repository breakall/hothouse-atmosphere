// SPDX-License-Identifier: GPL-3.0-or-later
#include "debug.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cerrno>
#include <climits>

namespace atmosphere {
namespace {
unsigned Quantize(float value) { return unsigned(Clamp(value, 0, 1) * 10000 + .5f); }
void Flatten(const Panel& p, unsigned* values) {
    for(unsigned i = 0; i < 6; ++i) values[i] = Quantize(p.knobs[i]);
    for(unsigned i = 0; i < 3; ++i) values[6+i] = p.toggles[i];
    for(unsigned i = 0; i < 2; ++i) values[9+i] = p.feet[i];
}
}

void SnapshotMailbox::Publish(const Snapshot& s) {
    unsigned data[kWords], n = 0;
    for(auto v : s.physical) data[n++] = v;
    for(auto v : s.effective) data[n++] = v;
    for(auto v : s.requested) data[n++] = v;
    data[n++] = s.mask; data[n++] = s.state; data[n++] = s.divider;
    data[n++] = s.peak; data[n++] = s.applied;
    version_.fetch_add(1, std::memory_order_acq_rel);
    for(unsigned i = 0; i < kWords; ++i) words_[i].store(data[i], std::memory_order_relaxed);
    version_.fetch_add(1, std::memory_order_release);
}
Snapshot SnapshotMailbox::Read() const {
    unsigned data[kWords];
    for(;;) {
        const unsigned before = version_.load(std::memory_order_acquire);
        if(before & 1) continue;
        for(unsigned i = 0; i < kWords; ++i) data[i] = words_[i].load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if(before == version_.load(std::memory_order_relaxed)) break;
    }
    Snapshot s; unsigned n = 0;
    for(auto& v : s.physical) v = data[n++];
    for(auto& v : s.effective) v = data[n++];
    for(auto& v : s.requested) v = data[n++];
    s.mask = data[n++]; s.state = data[n++]; s.divider = data[n++];
    s.peak = data[n++]; s.applied = data[n++];
    return s;
}
unsigned DebugControls::Observe(const Panel& p) {
    unsigned button_sources = 0;
    if(observed_) {
        for(unsigned i = 0; i < 6; ++i)
            if(Overridden(i) && std::fabs(p.knobs[i] - knob_anchor_[i]) >= .01f)
                mask_ &= ~(1u << i);
        for(unsigned i = 0; i < 3; ++i)
            if(Overridden(i+6) && p.toggles[i] != toggle_anchor_[i])
                mask_ &= ~(1u << (i+6));
        for(unsigned i = 0; i < 2; ++i) if(p.feet[i] != physical_.feet[i]) {
            if(Overridden(i+9)) button_sources |= 1u << i;
            // A real footswitch takes back its virtual button, on/off state
            // and hold function. Other preset controls remain unchanged.
            mask_ &= ~((1u << (i+9)) | (1u << (i+11)) | (1u << (i+13)));
        }
    }
    physical_ = p; observed_ = true;
    return button_sources;
}
void DebugControls::Apply(const Command& c) {
    if(c.operation == Operation::Set && c.field < kControls) {
        values_[c.field] = c.value; mask_ |= 1u << c.field;
        if(c.field < 6) knob_anchor_[c.field] = physical_.knobs[c.field];
        else if(c.field < 9) toggle_anchor_[c.field-6] = physical_.toggles[c.field-6];
    } else if(c.operation == Operation::Clear) {
        if(c.field == kControls) mask_ = 0;
        else if(c.field < kControls) mask_ &= ~(1u << c.field);
    }
    applied_ = c.id;
}
Panel DebugControls::Resolve(const Panel& physical) const {
    Panel result = physical;
    for(unsigned i = 0; i < 6; ++i) if(mask_ & (1u << i)) result.knobs[i] = values_[i] / 10000.f;
    for(unsigned i = 0; i < 3; ++i) if(mask_ & (1u << (i+6))) result.toggles[i] = values_[i+6];
    for(unsigned i = 0; i < 2; ++i) if(mask_ & (1u << (i+9))) result.feet[i] = values_[i+9] != 0;
    return result;
}
void DebugControls::Fill(Snapshot& s, const Panel& physical, const Panel& effective) const {
    Flatten(physical, s.physical); Flatten(effective, s.effective);
    for(unsigned i = 0; i < kControls; ++i) s.requested[i] = values_[i];
    s.mask = mask_; s.applied = applied_;
}

bool ParseCommand(const char* line, Command& out, const char*& error) {
    // A bounded copy also makes parser behavior independent of USB packet cuts.
    if(std::strlen(line) >= 128) { error = "line_too_long"; return false; }
    char copy[128]; std::strcpy(copy, line);
    char* words[4] = {}; unsigned count = 0; char* save = nullptr;
    for(char* w = ::strtok_r(copy, " \t\r", &save); w; w = ::strtok_r(nullptr, " \t\r", &save)) {
        if(count == 4) { error = "invalid_command"; return false; }
        words[count++] = w;
    }
    out = Command{};
    if(count == 1 && !std::strcmp(words[0], "get")) { out.operation = Operation::Get; return true; }
    if(count == 2 && !std::strcmp(words[0], "get")) {
        char* end; errno = 0;
        const unsigned long v = std::strtoul(words[1], &end, 10);
        if(words[1][0] < '0' || words[1][0] > '9' || *end || !v || v > UINT_MAX || errno == ERANGE) {
            error = "invalid_request_id"; return false;
        }
        out.operation = Operation::Get; out.value = unsigned(v); return true;
    }
    if(count == 1 && !std::strcmp(words[0], "ping")) { out.operation = Operation::Ping; return true; }
    if(count == 1 && !std::strcmp(words[0], "dfu")) { out.operation = Operation::Dfu; return true; }
    if(count == 2 && !std::strcmp(words[0], "watch")) {
        char* end; const long v = std::strtol(words[1], &end, 10);
        if(*end || (v != 0 && (v < 100 || v > 10000))) { error = "watch_range_100_to_10000_or_0"; return false; }
        out.operation = Operation::Watch; out.value = unsigned(v); return true;
    }
    if(count >= 2 && (!std::strcmp(words[0], "set") || !std::strcmp(words[0], "clear"))) {
        const bool set = !std::strcmp(words[0], "set");
        if(count != (set ? 3u : 2u)) { error = "invalid_command"; return false; }
        out.operation = set ? Operation::Set : Operation::Clear;
        if(!set && !std::strcmp(words[1], "all")) { out.field = kControls; return true; }
        for(unsigned i = 0; i < kControls; ++i) if(!std::strcmp(words[1], kControlNames[i])) out.field = i;
        if(out.field == kControls) { error = "unknown_control"; return false; }
        if(!set) return true;
        char* end; const float v = std::strtof(words[2], &end);
        if(end == words[2] || *end || !std::isfinite(v)) { error = "invalid_value"; return false; }
        const float high = out.field < 6 ? 1.f : out.field < 9 ? 2.f : 1.f;
        if(v < 0 || v > high || (out.field >= 6 && v != std::floor(v))) { error = "value_out_of_range"; return false; }
        out.value = out.field < 6 ? Quantize(v) : unsigned(v); return true;
    }
    error = "invalid_command"; return false;
}

size_t FormatSnapshot(char* buffer, size_t capacity, const Snapshot& s, unsigned request) {
    size_t used = 0; bool valid = true;
    auto append = [&](const char* format, ...) {
        if(!valid) return;
        va_list args; va_start(args, format);
        const int n = std::vsnprintf(buffer + used, capacity - used, format, args);
        va_end(args);
        if(n < 0 || size_t(n) >= capacity - used) { valid = false; return; }
        used += size_t(n);
    };
    append("{\"type\":\"state\",\"version\":\"%s\",\"request\":%u,\"applied\":%u,\"physical\":{", kVersion, request, s.applied);
    auto fields = [&](const unsigned* values) {
        for(unsigned i = 0; i < kPanelControls; ++i) {
            if(i) append(",");
            if(i < 6) append("\"%s\":%u.%04u", kControlNames[i], values[i]/10000, values[i]%10000);
            else append("\"%s\":%u", kControlNames[i], values[i]);
        }
    };
    fields(s.physical); append("},\"effective\":{"); fields(s.effective);
    append(",\"wet_on\":%s,\"loop_on\":%s,\"freeze\":%s,\"overdub\":%s},\"overrides\":{",
        s.state & 1 ? "true" : "false", s.state & 2 ? "true" : "false",
        s.state & 4 ? "true" : "false", s.state & 8 ? "true" : "false");
    bool first = true;
    for(unsigned i = 0; i < kControls; ++i) if(s.mask & (1u << i)) {
        if(!first) append(",");
        first = false;
        if(i < 6) append("\"%s\":%u.%04u", kControlNames[i], s.requested[i]/10000, s.requested[i]%10000);
        else append("\"%s\":%u", kControlNames[i], s.requested[i]);
    }
    append("},\"clock_divider\":%u,\"peak_cpu_permille\":%u}\r\n", s.divider, s.peak);
    return valid ? used : 0;
}

void CommandReceiver::Receive(const uint8_t* data, size_t count) {
    for(size_t i = 0; i < count; ++i) {
        if(dropping_) {
            if(data[i] == '\n' && bytes_.Push(256)) dropping_ = false;
        } else if(!bytes_.Push(data[i])) {
            dropping_ = true;
            // If this byte was the delimiter, recovery waits for a later one.
        }
    }
}
CommandReceiver::Result CommandReceiver::Next(char* line, size_t capacity) {
    uint16_t c;
    while(bytes_.Pop(c)) {
        if(c == 256) { used_ = 0; too_long_ = false; return Result::Overflow; }
        if(c == '\n') {
            if(too_long_) { used_ = 0; too_long_ = false; return Result::TooLong; }
            if(used_ >= capacity) { used_ = 0; return Result::TooLong; }
            std::memcpy(line, line_, used_); line[used_] = 0; used_ = 0; return Result::Line;
        }
        if(c == 0 || c > 127 || used_ + 1 >= sizeof(line_)) too_long_ = true;
        else if(!too_long_) line_[used_++] = char(c);
    }
    return Result::None;
}
} // namespace atmosphere
