// SPDX-License-Identifier: GPL-3.0-or-later
#include "debug.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
using namespace atmosphere;
int main() {
    const char* error = nullptr; Command c;
    assert(ParseCommand("get 4294967295",c,error) && c.value == 4294967295u);
    assert(!ParseCommand("get 4294967296",c,error));
    assert(ParseCommand("set mix 0.625", c, error) && c.field == 0 && c.value == 6250);
    DebugControls controls; c.id = 17; controls.Apply(c);
    Panel physical; physical.knobs[0] = .2f; physical.feet[1] = true;
    auto effective = controls.Resolve(physical);
    assert(effective.knobs[0] == .625f && effective.feet[1]);
    assert(ParseCommand("set loop_mode 2", c, error)); controls.Apply(c);
    assert(controls.Resolve(physical).toggles[2] == 2);
    assert(ParseCommand("set freeze 1", c, error)); controls.Apply(c);
    assert(controls.Boolean(13,false));
    assert(controls.Overridden(13) && !controls.Overridden(11));
    assert(ParseCommand("set fs1 1", c, error)); controls.Apply(c);
    assert(controls.Resolve(physical).feet[0]);
    for(const char* bad : {"set mix nan", "set mix inf", "set mix -0.1", "set mix 1.1",
        "set routing .5", "set routing 3", "set fs1 2", "set unknown 1", "set mix .5 extra",
        "watch 50", "watch -1", "set mix", "clear", "get extra"}) {
        assert(!ParseCommand(bad,c,error));
    }
    Snapshot s; controls.Fill(s,physical,controls.Resolve(physical));
    assert(s.physical[0] == 2000 && s.effective[0] == 6250 && s.mask != 0);
    s.state = 15; s.peak = 250; s.divider = 4;
    char json[1536]; assert(FormatSnapshot(json,sizeof(json),s));
    assert(std::strstr(json,"\"mix\":0.6250") && std::strstr(json,"\"freeze\":true"));
    char tiny[8]; assert(FormatSnapshot(tiny,sizeof(tiny),s) == 0);
    for(unsigned i = 0; i < kControls; ++i) {
        c.operation = Operation::Set; c.field = i; c.value = i < 6 ? 10000 : i < 9 ? 2 : 1; controls.Apply(c);
    }
    controls.Fill(s,physical,controls.Resolve(physical)); assert(FormatSnapshot(json,sizeof(json),s));
    std::puts(json); // Also consumed by the host's JSON validation check.
    assert(ParseCommand("clear mix",c,error)); controls.Apply(c);
    assert(controls.Resolve(physical).knobs[0] == physical.knobs[0]);
    assert(ParseCommand("clear all",c,error)); controls.Apply(c);
    controls.Fill(s,physical,controls.Resolve(physical)); assert(s.mask == 0 && !controls.Boolean(13,false));

    // A preset stays active indefinitely despite ADC noise. A slow physical
    // movement accumulates against its load-time position, not the last sample.
    DebugControls preset; Panel panel;
    for(auto& knob : panel.knobs) knob = .25f;
    preset.Observe(panel);
    auto set = [&](unsigned field, unsigned value) {
        Command command; command.operation=Operation::Set; command.field=field; command.value=value;
        preset.Apply(command);
    };
    for(unsigned i=0;i<6;++i) set(i,8000);
    set(6,2); set(7,2); set(8,2);
    for(unsigned frame=0;frame<60000;++frame) {
        Panel noise=panel;
        for(auto& knob : noise.knobs) knob += frame&1 ? .001f : -.001f;
        preset.Observe(noise);
        for(unsigned i=0;i<9;++i) assert(preset.Overridden(i));
    }
    for(unsigned step=1;step<=21;++step) {
        panel.knobs[0]=.25f + step*.0005f; preset.Observe(panel);
    }
    assert(!preset.Overridden(0) && preset.Overridden(1));
    assert(preset.Resolve(panel).knobs[0]==panel.knobs[0]);
    assert(preset.Resolve(panel).knobs[1]==.8f);
    set(0,9000); // Reload anchors at the current physical position.
    panel.knobs[0]-=.005f; preset.Observe(panel); assert(preset.Overridden(0));
    panel.knobs[0]-=.006f; preset.Observe(panel); assert(!preset.Overridden(0));
    panel.toggles[1]=1; preset.Observe(panel);
    assert(!preset.Overridden(7) && preset.Overridden(6) && preset.Overridden(8));
    // Physical switches recover their own button/state/hold without disturbing
    // the opposite switch or the recipe's other controls.
    for(unsigned i=9;i<15;++i) set(i,1);
    panel.feet[0]=true; assert(preset.Observe(panel)==1);
    assert(!preset.Overridden(9) && !preset.Overridden(11) && !preset.Overridden(13));
    assert(preset.Overridden(10) && preset.Overridden(12) && preset.Overridden(14));
    panel.feet[1]=true; assert(preset.Observe(panel)==2);
    assert(!preset.Overridden(10) && !preset.Overridden(12) && !preset.Overridden(14));
    assert(preset.Overridden(1));

    CommandReceiver receiver;
    auto receive = [&](const std::string& bytes) { receiver.Receive(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()); };
    char line[128];
    receive("set mi"); assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::None);
    receive("x .5\r\nget\n");
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::Line && std::string(line) == "set mix .5\r");
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::Line && std::string(line) == "get");
    receive(std::string(150,'x') + "\nget\n");
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::TooLong);
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::Line && std::string(line) == "get");
    receive(std::string(2100,'x') + "set mix 1\n");
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::None);
    receive("\nget\n");
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::Overflow);
    assert(receiver.Next(line,sizeof(line)) == CommandReceiver::Result::Line && std::string(line) == "get");

    SpscQueue<Command,3> queue;
    assert(queue.Push(c) && queue.Push(c) && !queue.Push(c));
    assert(queue.Pop(c) && queue.Pop(c) && !queue.Pop(c));
    SnapshotMailbox mailbox; std::atomic<bool> done{false};
    std::thread writer([&] {
        for(unsigned i=1;i<50000;++i) {
            Snapshot frame;
            for(auto& x : frame.physical) x=i;
            for(auto& x : frame.effective) x=i;
            frame.applied=i; mailbox.Publish(frame);
        }
        done.store(true);
    });
    do {
        const auto frame=mailbox.Read();
        for(auto x : frame.physical) assert(x==frame.applied);
        for(auto x : frame.effective) assert(x==frame.applied);
    } while(!done.load());
    writer.join();
    std::puts("PASS: USB parser, persistent presets, noise rejection, slow knob/toggle takeover, footswitch takeover, clear, packet fragmentation, overflow recovery, coherent concurrent snapshots");
}
