// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include "gestures.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace atmosphere;
constexpr float pi = 3.14159265358979323846f;
Stereo Tone(int i) { return {.2f * std::sin(2 * pi * 220 * i / 48000),
                            .1f * std::sin(2 * pi * 330 * i / 48000)}; }
float Energy(Stereo x) { return x.l*x.l + x.r*x.r; }
void Valid(Stereo x) { assert(std::isfinite(x.l) && std::isfinite(x.r)); assert(std::fabs(x.l) <= 1 && std::fabs(x.r) <= 1); }

int main() {
    auto e = std::make_unique<Engine>();
    e->Init();
    for(int i = 0; i < 10000; ++i) {
        const auto input = Tone(i), output = e->Process(input);
        assert(input.l == output.l && input.r == output.r);
    }
    // Capture pre-press history. Playback must continue with a silent input.
    Parameters p; p.mix = 1; p.length = 1; p.loop_modify = .7f;
    e->Configure(p); e->ToggleLoop();
    float sum = 0;
    for(int i = 0; i < 48000; ++i) { auto out = e->Process({}); Valid(out); if(i > 10000) sum += Energy(out); }
    assert(sum > 10);
    // At full mix, the stereo input must not leak into a silent captured loop.
    e->Init(); e->Configure(p);
    for(int i = 0; i < 24000; ++i) e->Process({});
    e->ToggleLoop();
    for(int i = 0; i < 10000; ++i) e->Process({});
    sum = 0;
    for(int i = 0; i < 24000; ++i) sum += Energy(e->Process(Tone(i)));
    assert(sum < 1e-8f);
    e->SetOverdub(true);
    for(int i = 0; i < 24000; ++i) e->Process(Tone(i));
    e->SetOverdub(false);
    sum = 0;
    for(int i = 0; i < 24000; ++i) sum += Energy(e->Process({}));
    assert(sum > 1);

    // Every mode/routing/clock combination, with live pot motion and holds.
    for(int wet = 0; wet < 3; ++wet) for(int loop = 0; loop < 3; ++loop)
    for(int route = 0; route < 3; ++route) for(int clock = 0; clock < 5; ++clock) {
        e->Init();
        p.wet_mode = WetMode(wet); p.loop_mode = LoopMode(loop); p.routing = Routing(route);
        p.clock = clock / 4.f; p.mix = .75f;
        e->Configure(p); e->ToggleWet();
        for(int i = 0; i < 40000; ++i) Valid(e->Process(Tone(i)));
        e->ToggleLoop();
        for(int i = 0; i < 24000; ++i) {
            if(i % 480 == 0) {
                p.time = p.length = float(i % 4800) / 4800;
                p.loop_modify = p.wet_modify = float(i % 9600) / 9600;
                e->Configure(p);
            }
            if(i == 4000) e->SetFreeze(true);
            if(i == 8000) e->SetFreeze(false);
            if(i == 12000) e->SetOverdub(true);
            if(i == 18000) e->SetOverdub(false);
            Valid(e->Process(Tone(i)));
        }
        e->ToggleLoop(); // Resume recording after overdub: physical ring pointer remains valid.
        for(int i = 0; i < 1000; ++i) Valid(e->Process(Tone(i)));
        e->ToggleLoop();
        for(int i = 0; i < 1000; ++i) Valid(e->Process({}));
    }
    // Silent boot, even if capture/freeze happen before any audio has arrived.
    e->Init(); e->ToggleLoop(); e->ToggleWet(); e->SetFreeze(true);
    for(int i = 0; i < 1000; ++i) { const auto out = e->Process({}); Valid(out); assert(Energy(out) == 0); }
    e->Init();
    for(int i = 0; i < 5; ++i) { p.clock = 1 - i / 4.f; e->Configure(p); assert(e->ClockDivider() == (1u << i)); }

    Gestures g;
    for(int i = 0; i < 100; ++i) g.Update(true, false);
    g.Update(false, false); assert(g.tap[0]);
    for(int i = 0; i < 500; ++i) g.Update(false, true);
    assert(g.hold[1]); g.Update(false, false); assert(!g.tap[1]);
    for(int i = 0; i < 2001; ++i) { g.Update(true, true); assert(!g.tap[0] && !g.tap[1] && !g.hold[0] && !g.hold[1]); }
    assert(g.boot);
    g.Update(false, true); g.Update(false, false); assert(!g.tap[1]);
    std::puts("PASS: stereo bypass, history capture, dry-only overdub, 135 mode/routing/clock combinations, empty capture, clock steps, gestures");
}
