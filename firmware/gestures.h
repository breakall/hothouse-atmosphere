// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace atmosphere {
// One update per audio block (1 ms). Tap fires on release so a hold never
// accidentally toggles bypass. A two-switch chord cancels both single gestures.
struct Gestures {
    struct Button { unsigned ticks = 0; bool down = false, held = false; } b[2];
    bool tap[2] = {}, hold[2] = {}, boot = false, chord = false;
    unsigned chord_ticks = 0;
    void Update(bool left, bool right) {
        tap[0] = tap[1] = hold[0] = hold[1] = false;
        const bool pressed[] = {left, right};
        if(left && right) chord = true;
        if(chord) {
            chord_ticks = left && right ? chord_ticks + 1 : 0;
            if(chord_ticks >= 2000) boot = true;
            for(auto& button : b) button = {};
            if(!left && !right) chord = false;
            return;
        }
        for(int i = 0; i < 2; ++i) {
            if(pressed[i]) {
                ++b[i].ticks;
                b[i].held = b[i].ticks >= 400;
                hold[i] = b[i].held;
            } else {
                tap[i] = b[i].down && !b[i].held;
                b[i] = {};
            }
            b[i].down = pressed[i];
        }
    }
};
} // namespace atmosphere
