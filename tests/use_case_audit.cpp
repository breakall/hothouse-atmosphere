// SPDX-License-Identifier: GPL-3.0-or-later
// Behavioral audit of the shipped engine. LIMIT lines deliberately record
// current missing behavior; they are not assertions that MOOD parity exists.
#include "engine.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
using namespace atmosphere;

Stereo Signal(int i) {
    return {.2f * std::sin(.001f * i) + .1f * std::sin(.017f * i),
            .15f * std::sin(.023f * i)};
}
float Difference(Stereo a, Stereo b) {
    return std::max(std::fabs(a.l - b.l), std::fabs(a.r - b.r));
}
float Energy(Stereo x) { return x.l*x.l + x.r*x.r; }
void ConfigureAtBlock(Engine& e, Parameters p, int i) { if(i % 48 == 0) e.Configure(p); }

int main() {
    auto a = std::make_unique<Engine>(), b = std::make_unique<Engine>();
    Parameters p; p.mix = 1; p.wet_mode = WetMode::Slip;
    auto q = p; p.time = .1f; q.time = .9f;
    a->Init(); b->Init(); a->ToggleWet(); b->ToggleWet();
    float delta = 0;
    for(int i = 0; i < 48000; ++i) {
        ConfigureAtBlock(*a,p,i); ConfigureAtBlock(*b,q,i);
        const auto x=a->Process(Signal(i)), y=b->Process(Signal(i));
        if(i > 10000) delta=std::max(delta,Difference(x,y));
    }
    assert(delta == 0);
    std::printf("LIMIT slip_time: TIME 10%% vs 90%% after fresh boot gives max output difference %.9f\n",delta);

    // Same freeze, followed by opposite pitch/direction commands.
    a->Init(); b->Init(); p.time=q.time=.5f; p.wet_modify=q.wet_modify=.8f;
    a->ToggleWet(); b->ToggleWet();
    for(int i=0;i<48000;++i) {
        ConfigureAtBlock(*a,p,i); ConfigureAtBlock(*b,q,i);
        a->Process(Signal(i)); b->Process(Signal(i));
    }
    a->SetFreeze(true); b->SetFreeze(true); p.wet_modify=0; q.wet_modify=1;
    delta=0; float frozen_energy=0;
    for(int i=0;i<24000;++i) {
        ConfigureAtBlock(*a,p,i); ConfigureAtBlock(*b,q,i);
        const auto x=a->Process({}), y=b->Process({});
        frozen_energy+=Energy(x); delta=std::max(delta,Difference(x,y));
    }
    assert(frozen_energy>1 && delta==0);
    std::printf("LIMIT frozen_edit: reverse vs forward Slip MODIFY gives max difference %.9f; captured audio remains audible\n",delta);

    // Stereo input is retained; Tape does not generate opposite directions or
    // a new right channel when the source exists only on the left.
    a->Init(); p=Parameters{}; p.mix=1; p.length=1; p.loop_modify=.69f;
    a->Configure(p);
    for(int i=0;i<24000;++i) a->Process({Signal(i).l,0});
    a->ToggleLoop(); float left=0,right=0;
    for(int i=0;i<48000;++i) { auto out=a->Process({}); if(i>5000) {left+=out.l*out.l;right+=out.r*out.r;} }
    assert(left>1 && right==0);
    std::printf("LIMIT tape_spread: left-only capture produces right-channel energy %.9f\n",right);

    // Full-wet stereo bypass should not continue a wet tail after settling.
    a->Init(); p=Parameters{}; p.mix=1; p.time=.7f;
    a->Configure(p); a->ToggleWet();
    for(int i=0;i<24000;++i) a->Process(Signal(i));
    a->ToggleWet(); float tail=0;
    for(int i=0;i<24000;++i) {auto out=a->Process({}); if(i>4800) tail+=Energy(out);}
    assert(tail==0);
    std::printf("LIMIT bypass_trails: audible tail energy after 100 ms bypass = %.9f\n",tail);

    // Stretch at noon should repeatedly play a stationary grain rather than
    // progressing through the capture. Grain length is 4096 for LENGTH=50%%.
    a->Init(); p=Parameters{}; p.mix=1; p.loop_mode=LoopMode::Stretch;
    p.length=.5f; p.loop_modify=.5f; a->Configure(p);
    for(int i=0;i<24000;++i) a->Process(Signal(i));
    a->ToggleLoop(); std::vector<Stereo> grain(4096); delta=0;
    for(int i=0;i<24000;++i) {
        const auto out=a->Process({});
        if(i>=16384) delta=std::max(delta,Difference(out,grain[i%4096]));
        grain[i%4096]=out;
    }
    assert(delta<.0001f);
    std::printf("SUPPORTED stretch_hold: consecutive stationary grains differ by at most %.9f\n",delta);

    a->Init(); p=Parameters{}; p.mix=1; p.loop_mode=LoopMode::Envelope;
    p.length=0; p.loop_modify=1; a->Configure(p);
    for(int i=0;i<24000;++i) a->Process(Signal(i));
    a->ToggleLoop(); std::vector<Stereo> slice(48); delta=0;
    for(int i=0;i<12000;++i) {
        const auto out=a->Process({.01f,.01f});
        if(i>10000) delta=std::max(delta,Difference(out,slice[i%48]));
        slice[i%48]=out;
    }
    assert(delta<.0001f);
    float released_delta=0;
    for(int i=0;i<24000;++i) {
        const auto out=a->Process({});
        if(i>10000) released_delta=std::max(released_delta,Difference(out,slice[i%48]));
        slice[i%48]=out;
    }
    assert(released_delta>.01f);
    std::printf("SUPPORTED envelope_stutter: held slice difference %.9f; silence releases progression (difference %.6f)\n",delta,released_delta);

    // A tail can be re-captured without live input, regardless of routing.
    a->Init(); p=Parameters{}; p.mix=1; p.time=.9f; p.wet_modify=.9f;
    p.routing=Routing::Loop; p.loop_modify=.69f; p.length=1;
    a->Configure(p); a->ToggleWet();
    for(int i=0;i<48000;++i) a->Process(Signal(i));
    a->ToggleLoop();
    for(int i=0;i<24000;++i) a->Process({});
    a->ToggleLoop(); // listening replaces the capture with dry + wet tail
    for(int i=0;i<12000;++i) a->Process({});
    a->ToggleLoop(); a->ToggleWet(); float caught_tail=0;
    for(int i=0;i<24000;++i) {const auto out=a->Process({}); if(i>5000)caught_tail+=Energy(out);}
    assert(caught_tail>.01f);
    std::printf("SUPPORTED wet_recapture: resampled tail energy with live input silent = %.6f\n",caught_tail);

    // Nominally unlimited delay feedback still decays without any new input.
    a->Init(); p=Parameters{}; p.mix=1; p.wet_mode=WetMode::Delay;
    p.time=std::sqrt((4800.f-96)/44000); p.wet_modify=1;
    a->Configure(p); a->ToggleWet();
    for(int i=0;i<5000;++i) { ConfigureAtBlock(*a,p,i); a->Process({}); }
    float early=0,late=0;
    for(int i=0;i<4800*110;++i) {
        const auto out=a->Process(i==0?Stereo{.5f,0}:Stereo{});
        if(i>=4800 && i<4800*2) early=std::max(early,std::fabs(out.l));
        if(i>=4800*100 && i<4800*101) late=std::max(late,std::fabs(out.l));
    }
    assert(early>0 && late<early*.7f);
    std::printf("LIMIT delay_infinite: repeat 100 / repeat 1 amplitude = %.6f\n",late/early);
}
