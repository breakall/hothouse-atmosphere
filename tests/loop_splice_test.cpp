// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include <cstdio>
#include <memory>
using namespace atmosphere;

int main() {
    auto engine = std::make_unique<Engine>();
    engine->Init();
    Parameters p;
    p.mix = 1; p.clock = 1; p.length = 1;
    p.loop_mode = LoopMode::Tape; p.loop_modify = .69f;
    engine->Configure(p);
    // A clean, non-period-aligned stereo capture: normal guitar notes rarely
    // line up with the looper's fixed duration. The original waveform has
    // adjacent-sample changes below .006, so larger jumps are loop artifacts.
    auto input = [](int frame) -> Stereo {
        return {.3f * std::sin(.113f + 6.2831853f * 137.2f * frame / 48000),
                .2f * std::sin(.317f + 6.2831853f * 193.7f * frame / 48000)};
    };
    Stereo previous;
    for(int i = 0; i < 24000; ++i) previous = engine->Process(input(i));
    engine->ToggleLoop();
    float onset_jump = 0, repeat_jump = 0;
    int worst_frame = 0;
    for(int i = 0; i < 72000; ++i) {
        Stereo out = engine->Process(input(24000 + i));
        const float jump = std::max(std::fabs(out.l - previous.l), std::fabs(out.r - previous.r));
        if(i < 2000) onset_jump = std::max(onset_jump, jump);
        else if(jump > repeat_jump) { repeat_jump = jump; worst_frame = i; }
        previous = out;
    }
    std::printf("capture max jump=%.6f; repeat max jump=%.6f at frame %d (limit .020)\n", onset_jump, repeat_jump, worst_frame);
    if(onset_jump >= .020f || repeat_jump >= .020f) return 1;

    // Exercise the same recorded-buffer seam through every loop reader,
    // both directions and all eight tape speeds, at full and cropped length.
    // Capture after the ring has wrapped, so its physical start is nonzero.
    float worst = 0;
    for(int mode = 0; mode < 3; ++mode) for(int speed = 0; speed < 8; ++speed)
    for(float length : {.4f, 1.f}) {
        engine->Init();
        p.loop_mode = LoopMode(mode); p.length = length;
        p.loop_modify = (speed + .5f) / 8;
        engine->Configure(p);
        for(int i = 0; i < 31729; ++i) engine->Process(input(i));
        engine->ToggleLoop();
        previous = {};
        for(int i = 0; i < 72000; ++i) {
            const Stereo out = engine->Process({});
            const float jump = std::max(std::fabs(out.l - previous.l), std::fabs(out.r - previous.r));
            if(i > 2000) {
                worst = std::max(worst, jump);
                if(jump > .045f) {
                    std::printf("FAIL mode=%d speed zone=%d length=%.1f jump=%.6f frame=%d\n", mode, speed, length, jump, i);
                    return 1;
                }
            }
            previous = out;
        }
    }
    std::printf("PASS: 48 loop mode/speed/length cases; worst sample jump %.6f\n", worst);
    return 0;
}
