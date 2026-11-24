// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace atmosphere {
struct Stereo {
    float l = 0, r = 0;
    Stereo operator+(Stereo b) const { return {l + b.l, r + b.r}; }
    Stereo operator-(Stereo b) const { return {l - b.l, r - b.r}; }
    Stereo operator*(float gain) const { return {l * gain, r * gain}; }
};
inline float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }
inline float Wrap(float p, float size) {
    // All heads advance by at most four frames per tick; fmod also handles
    // parameter changes that shrink a region underneath a running head.
    p = std::fmod(p, size);
    return p < 0 ? p + size : p;
}
inline Stereo Protect(Stereo s) { return {Clamp(s.l, -1, 1), Clamp(s.r, -1, 1)}; }
template <size_t N> struct Ring {
    Stereo frames[N];
    size_t write = 0;
    void Clear() { std::fill_n(frames, N, Stereo{}); write = 0; }
    void Push(Stereo s) { frames[write] = s; write = (write + 1) % N; }
    Stereo At(float p) const {
        p = Wrap(p, float(N));
        const size_t a = size_t(p), b = (a + 1) % N;
        return frames[a] + (frames[b] - frames[a]) * (p - float(a));
    }
    Stereo Behind(float delay) const { return At(float(write) - delay); }
};
enum class WetMode { Reverb, Delay, Slip };
enum class LoopMode { Envelope, Tape, Stretch };
enum class Routing { Input, Both, Loop };
struct Parameters {
    float mix = .5f, clock = 1, time = .5f, wet_modify = .5f;
    float length = .5f, loop_modify = .75f;
    WetMode wet_mode = WetMode::Reverb;
    LoopMode loop_mode = LoopMode::Tape;
    Routing routing = Routing::Both;
};

// Portable DSP: no peripheral access, allocation, or blocking work in Process.
// Init must run after SDRAM initialization when this object lives on a Daisy.
class Engine {
  public:
    static constexpr size_t kLoopFrames = 24000;
    static constexpr size_t kWetFrames = 48000;
    void Init();
    void Configure(Parameters p);
    void ToggleWet() { wet_on_ = !wet_on_; transition_from_ = held_; transition_ = 0; }
    void ToggleLoop();
    void SetFreeze(bool on);
    void SetOverdub(bool on) { overdub_ = on && loop_on_; }
    Stereo Process(Stereo input);
    bool WetOn() const { return wet_on_; }
    bool LoopOn() const { return loop_on_; }
    bool Frozen() const { return freeze_; }
    bool Overdubbing() const { return overdub_; }
    float RecordPhase() const { return float(record_.write) / float(kLoopFrames); }
    unsigned ClockDivider() const { return divider_; }
  private:
    Stereo Tick(Stereo input);
    Stereo Loop(Stereo input);
    Stereo Wet(Stereo input);
    Stereo LoopRead(float position, float length) const;
    float PlaybackRate(float knob, bool chromatic) const;
    Parameters p_;
    Ring<kLoopFrames> record_;
    Ring<kWetFrames> wet_, freeze_history_;
    Ring<2048> diffusion_[2];
    size_t recorded_ = 0, loop_start_ = 0, loop_size_ = 1, dub_write_ = 0;
    size_t freeze_start_ = 0, freeze_size_ = 1;
    float play_ = 0, transport_ = 0, grain_phase_ = 0;
    float grain_anchor_[2] = {0, 0};
    float env_ = 0, slice_start_ = 0;
    bool env_held_ = false;
    float delay_a_ = 12000, delay_b_ = 12000, delay_fade_ = 1;
    float slip_phase_ = 0, freeze_pos_ = 0;
    Stereo filter_, held_, previous_, transition_from_;
    float mix_ = 0, effect_gain_ = 0, transition_ = 1;
    unsigned divider_ = 1, tick_count_ = 0;
    bool wet_on_ = false, loop_on_ = false, freeze_ = false, overdub_ = false;
};
} // namespace atmosphere
