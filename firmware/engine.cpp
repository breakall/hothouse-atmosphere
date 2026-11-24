// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"

namespace atmosphere {
namespace {
float SpliceWidth(float length) { return std::min(240.f, length * .125f); }
float SplicedLength(float length) { return length - SpliceWidth(length); }

template <size_t N>
Stereo ReadSpliced(const Ring<N>& ring, size_t start, float position, float length) {
    const float seam = SpliceWidth(length);
    const float period = length - seam;
    position = Wrap(position, period);
    auto at = [&](float x) {
        const size_t a = size_t(x);
        const size_t b = a + 1 < size_t(std::ceil(length)) ? a + 1 : 0;
        const Stereo first = ring.frames[(start + a) % N];
        return first + (ring.frames[(start + b) % N] - first) * (x - float(a));
    };
    if(position >= seam) return at(position);
    // Overlap the captured tail with the head. At the wrap both directions
    // meet the same tail sample; the tail/head overlap is consumed only once.
    // The previous code faded toward head[seam], then jumped to head[0].
    const float blend = position / seam;
    return at(period + position) * (1 - blend) + at(position) * blend;
}
} // namespace

void Engine::Init() {
    // Reset scalar members without constructing a multi-megabyte stack object.
    record_.Clear(); wet_.Clear(); freeze_history_.Clear();
    for(auto& d : diffusion_) d.Clear();
    recorded_ = loop_start_ = dub_write_ = 0; loop_size_ = freeze_size_ = 1;
    freeze_start_ = 0;
    play_ = transport_ = grain_phase_ = env_ = slice_start_ = 0;
    grain_anchor_[0] = grain_anchor_[1] = 0;
    env_held_ = false;
    delay_a_ = delay_b_ = 12000; delay_fade_ = 1;
    slip_phase_ = freeze_pos_ = 0;
    filter_ = held_ = previous_ = transition_from_ = {};
    mix_ = effect_gain_ = 0; transition_ = 1;
    divider_ = 1; tick_count_ = 0;
    wet_on_ = loop_on_ = freeze_ = overdub_ = false;
    p_ = Parameters{};
    Configure(Parameters{});
}

void Engine::Configure(Parameters p) {
    p.mix = Clamp(p.mix, 0, 1); p.clock = Clamp(p.clock, 0, 1);
    p.time = Clamp(p.time, 0, 1); p.wet_modify = Clamp(p.wet_modify, 0, 1);
    p.length = Clamp(p.length, 0, 1); p.loop_modify = Clamp(p.loop_modify, 0, 1);
    if(p.wet_mode != p_.wet_mode || p.loop_mode != p_.loop_mode || p.routing != p_.routing) {
        transition_from_ = held_; transition_ = 0;
        env_held_ = false;
    }
    p_ = p;
    // Virtual clock, 48 / 24 / 12 / 6 / 3 kHz; physical codec stays at 48 kHz.
    // Hysteresis keeps an unmoving pot from chattering at an octave boundary.
    int step = 0;
    for(unsigned d = divider_; d > 1; d >>= 1) ++step;
    const float desired = (1 - p.clock) * 4;
    while(step < 4 && desired > float(step) + .58f) ++step;
    while(step > 0 && desired < float(step) - .58f) --step;
    divider_ = 1u << step;
    const float target = 96 + p.time * p.time * 44000;
    // Move between two fixed delay heads, avoiding a continuous Doppler glide.
    if(delay_fade_ >= 1 && std::fabs(target - delay_b_) > 64) {
        delay_a_ = delay_b_; delay_b_ = target; delay_fade_ = 0;
    }
}

void Engine::ToggleLoop() {
    transition_from_ = held_; transition_ = 0;
    loop_on_ = !loop_on_;
    overdub_ = false;
    if(loop_on_) {
        loop_size_ = std::max(size_t(1), recorded_);
        loop_start_ = (record_.write + kLoopFrames - loop_size_) % kLoopFrames;
        play_ = transport_ = grain_phase_ = 0;
        grain_anchor_[0] = grain_anchor_[1] = 0;
        dub_write_ = 0;
        env_held_ = false;
    } else {
        // Resuming the ring replaces the captured audio in chronological order.
        recorded_ = 0;
    }
}

void Engine::SetFreeze(bool on) {
    on = on && wet_on_;
    if(on && !freeze_) {
        freeze_size_ = size_t(Clamp(delay_b_, 96, float(kWetFrames - 1)));
        freeze_start_ = (freeze_history_.write + kWetFrames - freeze_size_) % kWetFrames;
        freeze_pos_ = 0;
    }
    freeze_ = on;
}

float Engine::PlaybackRate(float knob, bool chromatic) const {
    if(chromatic) {
        const float x = knob * 2 - 1;
        const float semitones = std::round((std::fabs(x) * 2 - 1) * 12);
        return (x < 0 ? -1.f : 1.f) * std::exp2(semitones / 12);
    }
    static constexpr float rates[8] = {-4, -2, -1, -.5f, .5f, 1, 2, 4};
    return rates[std::min(7, int(knob * 8))];
}

Stereo Engine::LoopRead(float pos, float length) const {
    return ReadSpliced(record_, loop_start_, pos, length);
}

Stereo Engine::Loop(Stereo input) {
    if(!loop_on_) return {};
    const float energy = std::max(std::fabs(input.l), std::fabs(input.r));
    env_ += (energy - env_) * (energy > env_ ? .02f : .0008f);
    const float full = float(loop_size_);
    Stereo result;
    if(p_.loop_mode == LoopMode::Tape) {
        const float length = std::max(1.f, full * (.02f + .98f * p_.length * p_.length));
        result = LoopRead(play_, length);
        play_ = Wrap(play_ + PlaybackRate(p_.loop_modify, false), SplicedLength(length));
        transport_ = play_;
    } else if(p_.loop_mode == LoopMode::Envelope) {
        const float slice = std::min(full, 48 + p_.length * p_.length * (full - 1));
        const float threshold = .25f * std::exp2(-9 * p_.loop_modify);
        const bool detected = env_ > threshold * (env_held_ ? .7f : 1.f);
        if(detected && !env_held_) { slice_start_ = transport_; play_ = 0; }
        env_held_ = detected;
        if(detected) {
            result = LoopRead(slice_start_ + play_, full);
            // Fade slice edges without muting an entire short captured loop.
            const float edge = std::min(32.f, slice * .125f);
            result = result * std::min(1.f, std::min(play_, slice - play_) / edge);
            play_ = Wrap(play_ + 1, slice);
        } else {
            result = LoopRead(transport_, full);
            transport_ = Wrap(transport_ + 1, SplicedLength(full));
        }
    } else {
        const float grain = std::min(full, 96 + p_.length * p_.length * 16000);
        const float speed = (p_.loop_modify * 2 - 1);
        const float scan = speed * std::fabs(speed);
        // Two overlapping triangular windows. Grain playback stays at unity
        // pitch while the anchor moves slowly, stops, or runs backward.
        const float phase[2] = {grain_phase_, Wrap(grain_phase_ + .5f, 1)};
        for(int g = 0; g < 2; ++g) {
            const float weight = 1 - std::fabs(phase[g] * 2 - 1);
            result = result + LoopRead(grain_anchor_[g] + phase[g] * grain, full) * weight;
        }
        transport_ = Wrap(transport_ + scan, SplicedLength(full));
        const float next = grain_phase_ + 1 / grain;
        if(grain_phase_ < .5f && next >= .5f) grain_anchor_[1] = transport_;
        if(next >= 1) grain_anchor_[0] = transport_;
        grain_phase_ = Wrap(next, 1);
        play_ = transport_;
    }
    if(overdub_) {
        // Chronological write head, independent of reversed/stretched playback.
        // Only dry input enters this feedback path.
        auto& frame = record_.frames[(loop_start_ + dub_write_) % kLoopFrames];
        frame = Protect(frame * .9995f + input * .5f);
        dub_write_ = (dub_write_ + 1) % loop_size_;
    }
    return result;
}

Stereo Engine::Wet(Stereo input) {
    Stereo result;
    if(freeze_) {
        // Replaying captured wet output freezes all three algorithms without
        // an unstable unity-feedback network.
        result = ReadSpliced(freeze_history_, freeze_start_, freeze_pos_, float(freeze_size_));
        freeze_pos_ = Wrap(freeze_pos_ + 1, SplicedLength(float(freeze_size_)));
        return result;
    }
    if(p_.wet_mode == WetMode::Delay) {
        delay_fade_ = std::min(1.f, delay_fade_ + 1.f / 960);
        result = wet_.Behind(delay_a_) * (1 - delay_fade_) + wet_.Behind(delay_b_) * delay_fade_;
        wet_.Push(Protect(input + result * (.995f * p_.wet_modify)));
    } else if(p_.wet_mode == WetMode::Slip) {
        const float grain = std::max(96.f, delay_b_ * .5f);
        const float rate = PlaybackRate(p_.wet_modify, true);
        for(int g = 0; g < 2; ++g) {
            const float phase = Wrap(slip_phase_ + g * .5f, 1);
            const float delay = 8 + grain + (1 - rate) * (phase - .5f) * grain;
            const float weight = 1 - std::fabs(phase * 2 - 1);
            result = result + wet_.Behind(Clamp(delay, 2, float(kWetFrames - 2))) * weight;
        }
        slip_phase_ = Wrap(slip_phase_ + 1 / grain, 1);
        wet_.Push(input);
    } else {
        // A stereo echo cluster with variable diffusion, rather than a fixed
        // room reverb: minimum MODIFY exposes individual taps.
        static constexpr float taps[] = {.109f, .173f, .277f, .419f, .613f, .797f, 1};
        const float size = 240 + p_.time * p_.time * 18000;
        for(size_t i = 0; i < 7; ++i) {
            Stereo tap = wet_.Behind(size * taps[i]);
            if(i & 1) tap = {tap.r, tap.l};
            result = result + tap * (1.f / 7);
        }
        for(size_t i = 0; i < 2; ++i) {
            const Stereo delayed = diffusion_[i].Behind(i ? 1429 : 887);
            const float coefficient = p_.wet_modify * .72f;
            const Stereo diffused = delayed - result * coefficient;
            diffusion_[i].Push(result + diffused * coefficient);
            result = result * (1 - p_.wet_modify) + diffused * p_.wet_modify;
        }
        // Stereo orthogonal feedback mixing and damping keep the tail bounded.
        filter_ = filter_ + (result - filter_) * (.85f - .7f * p_.wet_modify);
        const Stereo spread = {(filter_.l + filter_.r) * .70710678f,
                               (filter_.l - filter_.r) * .70710678f};
        wet_.Push(Protect(input + spread * (.35f + .6f * p_.time)));
    }
    freeze_history_.Push(result);
    return result;
}

Stereo Engine::Tick(Stereo input) {
    Stereo loop = Loop(input);
    Stereo feed = input;
    if(loop_on_) {
        if(p_.routing == Routing::Loop) feed = loop;
        if(p_.routing == Routing::Both) feed = (input + loop) * .70710678f;
    }
    Stereo wet = Wet(wet_on_ ? feed : Stereo{});
    if(!loop_on_) {
        record_.Push(Protect(input + (wet_on_ ? wet * .5f : Stereo{})));
        recorded_ = std::min(recorded_ + 1, kLoopFrames);
    }
    Stereo processed = loop;
    if(wet_on_) {
        processed = wet;
        if(loop_on_ && p_.routing == Routing::Input) processed = (wet + loop) * .70710678f;
    }
    if(transition_ < 1) {
        transition_ = std::min(1.f, transition_ + 1.f / 240);
        processed = transition_from_ * (1 - transition_) + processed * transition_;
    }
    return processed;
}

Stereo Engine::Process(Stereo input) {
    if(++tick_count_ >= divider_) {
        tick_count_ = 0;
        previous_ = held_;
        held_ = Tick(input);
    }
    const float fraction = float(tick_count_) / float(divider_);
    const Stereo wet = previous_ + (held_ - previous_) * fraction;
    mix_ += (p_.mix - mix_) * .002f;
    if(std::fabs(p_.mix - mix_) < .0001f) mix_ = p_.mix;
    const float target = wet_on_ || loop_on_ ? 1.f : 0.f;
    effect_gain_ += (target - effect_gain_) * .002f;
    if(std::fabs(target - effect_gain_) < .0001f) effect_gain_ = target;
    const float amount = mix_ * effect_gain_;
    // Linear blend gives exact dry bypass and avoids boosting correlated audio.
    return Protect(input * (1 - amount) + wet * amount);
}
} // namespace atmosphere
