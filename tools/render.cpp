// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
using namespace atmosphere;
void U16(std::ofstream& f, uint16_t x) { f.put(char(x)); f.put(char(x >> 8)); }
void U32(std::ofstream& f, uint32_t x) { U16(f, uint16_t(x)); U16(f, uint16_t(x >> 16)); }
int main(int argc, char** argv) {
    if(argc != 2) return 1;
    auto e = std::make_unique<Engine>();
    const char* wet_names[] = {"reverb", "delay", "slip"};
    const char* loop_names[] = {"env", "tape", "stretch"};
    for(int w = 0; w < 3; ++w) for(int l = 0; l < 3; ++l) {
        e->Init(); Parameters p;
        p.wet_mode = WetMode(w); p.loop_mode = LoopMode(l); p.time = .25f;
        p.mix = .65f; p.loop_modify = l == 1 ? .7f : .6f;
        e->Configure(p); e->ToggleWet();
        const std::string path = std::string(argv[1]) + "/" + wet_names[w] + "-" + loop_names[l] + ".wav";
        std::ofstream f(path, std::ios::binary);
        constexpr int frames = 48000 * 8, bytes = frames * 4;
        f.write("RIFF", 4); U32(f, 36 + bytes); f.write("WAVEfmt ", 8);
        U32(f, 16); U16(f, 1); U16(f, 2); U32(f, 48000); U32(f, 192000);
        U16(f, 4); U16(f, 16); f.write("data", 4); U32(f, bytes);
        for(int i = 0; i < frames; ++i) {
            if(i == 48000 * 2) e->ToggleLoop();
            if(i == 48000 * 4) { p.clock = .5f; e->Configure(p); }
            if(i == 48000 * 6) e->SetFreeze(true);
            const float t = float(i) / 48000;
            const float beat = std::fmod(t, .375f);
            const float note = 110 * std::exp2(float((i / 18000) % 5) / 12);
            const float a = .22f * std::exp(-beat * 15);
            const Stereo input = i < 48000 * 3 ? Stereo{a * (std::sin(6.2831853f * note * t) + .3f * std::sin(12.5663706f * note * t)),
                a * std::sin(6.2831853f * note * 1.5f * t)} : Stereo{};
            auto out = e->Process(input);
            U16(f, uint16_t(int16_t(Clamp(out.l, -1, 1) * 32767)));
            U16(f, uint16_t(int16_t(Clamp(out.r, -1, 1) * 32767)));
        }
        if(!f) return 1;
        std::printf("%s\n", path.c_str());
    }
}
