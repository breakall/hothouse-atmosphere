// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine.h"
#include "gestures.h"
#include "debug.h"
#include "usb_output.h"
#include "hothouse.h"
#include "util/CpuLoadMeter.h"
#include <atomic>

using clevelandmusicco::Hothouse;
using namespace atmosphere;
Hothouse hw;
// Large buffers live in external SDRAM; initialize explicitly after hw.Init().
Engine DSY_SDRAM_BSS engine;
Gestures gestures;
daisy::Led leds[2];
daisy::CpuLoadMeter load;
daisy::UsbHandle usb;
CommandReceiver receiver;
SpscQueue<Command, 32> commands;
DebugControls debug_controls;
SnapshotMailbox snapshots;
UsbOutput output;
unsigned recovery_ticks = 0;
bool wait_for_release = false;
std::atomic<bool> boot_requested{false};
std::atomic<unsigned> peak_load{0};

void Receive(uint8_t* data, uint32_t* bytes) { receiver.Receive(data, *bytes); }

int Toggle(Hothouse::Toggleswitch toggle) {
    const auto position = hw.GetToggleswitchPosition(toggle);
    return position == Hothouse::TOGGLESWITCH_UP ? 0
        : position == Hothouse::TOGGLESWITCH_DOWN ? 2 : 1;
}

void Audio(daisy::AudioHandle::InputBuffer in, daisy::AudioHandle::OutputBuffer out, size_t size) {
    load.OnBlockStart();
    hw.ProcessAllControls();
    Panel physical;
    for(unsigned i = 0; i < 6; ++i) physical.knobs[i] = hw.GetKnobValue(static_cast<Hothouse::Knob>(i));
    for(unsigned i = 0; i < 3; ++i) physical.toggles[i] = Toggle(static_cast<Hothouse::Toggleswitch>(i));
    physical.feet[0] = hw.switches[Hothouse::FOOTSWITCH_1].Pressed();
    physical.feet[1] = hw.switches[Hothouse::FOOTSWITCH_2].Pressed();
    const unsigned button_sources = debug_controls.Observe(physical);
    if(button_sources) {
        for(unsigned i = 0; i < 2; ++i) if(button_sources & (1u << i)) gestures.b[i] = {};
        gestures.chord = false; gestures.chord_ticks = 0;
    }
    // Recovery uses physical input exclusively; USB can neither suppress it
    // nor accidentally request DFU by simulating both switches.
    recovery_ticks = physical.feet[0] && physical.feet[1] ? recovery_ticks + 1 : 0;
    if(recovery_ticks >= 2000) boot_requested.store(true, std::memory_order_relaxed);
    Command command;
    while(commands.Pop(command)) {
        debug_controls.Apply(command);
        if(command.operation == Operation::Clear && (command.field == kControls || command.field == 9 || command.field == 10)) {
            gestures = {}; wait_for_release = true;
        }
    }
    Panel effective = debug_controls.Resolve(physical);
    if(wait_for_release) {
        wait_for_release = effective.feet[0] || effective.feet[1];
        effective.feet[0] = effective.feet[1] = false;
    }
    Parameters p;
    p.mix = effective.knobs[0]; p.time = effective.knobs[1];
    p.wet_modify = effective.knobs[2]; p.clock = effective.knobs[3];
    p.length = effective.knobs[4]; p.loop_modify = effective.knobs[5];
    p.wet_mode = static_cast<WetMode>(effective.toggles[0]);
    p.routing = static_cast<Routing>(effective.toggles[1]);
    p.loop_mode = static_cast<LoopMode>(effective.toggles[2]);
    engine.Configure(p);
    gestures.Update(effective.feet[0], effective.feet[1]);
    if(gestures.tap[0] && !debug_controls.Overridden(11)) engine.ToggleWet();
    if(gestures.tap[1] && !debug_controls.Overridden(12)) engine.ToggleLoop();
    if(debug_controls.Boolean(11, engine.WetOn()) != engine.WetOn()) engine.ToggleWet();
    if(debug_controls.Boolean(12, engine.LoopOn()) != engine.LoopOn()) engine.ToggleLoop();
    engine.SetFreeze(debug_controls.Boolean(13, gestures.hold[0]));
    engine.SetOverdub(debug_controls.Boolean(14, gestures.hold[1]));

    for(size_t i = 0; i < size; ++i) {
        const auto result = engine.Process({in[0][i], in[1][i]});
        out[0][i] = result.l;
        out[1][i] = result.r;
    }
    const bool blink = (daisy::System::GetNow() / 100) % 2;
    leds[0].Set(engine.Frozen() ? (blink ? 1 : .1f) : engine.WetOn() ? 1 : 0);
    leds[1].Set(engine.Overdubbing() ? (blink ? 1 : .1f)
        : engine.LoopOn() ? 1 : engine.RecordPhase() < .08f ? .2f : 0);
    for(auto& led : leds) led.Update();
    Snapshot snapshot;
    debug_controls.Fill(snapshot, physical, effective);
    snapshot.state = (engine.WetOn() ? 1u : 0u) | (engine.LoopOn() ? 2u : 0u)
        | (engine.Frozen() ? 4u : 0u) | (engine.Overdubbing() ? 8u : 0u);
    snapshot.divider = engine.ClockDivider(); snapshot.peak = peak_load.load(std::memory_order_relaxed);
    snapshots.Publish(snapshot);
    load.OnBlockEnd();
    const auto percent = static_cast<unsigned>(load.GetMaxCpuLoad() * 1000);
    if(percent > peak_load.load(std::memory_order_relaxed)) peak_load.store(percent, std::memory_order_relaxed);
}

int main() {
    hw.Init();
    hw.SetAudioSampleRate(daisy::SaiHandle::Config::SampleRate::SAI_48KHZ);
    hw.SetAudioBlockSize(48);
    engine.Init();
    load.Init(hw.AudioSampleRate(), hw.AudioBlockSize());
    leds[0].Init(hw.seed.GetPin(Hothouse::LED_1), false, hw.AudioCallbackRate());
    leds[1].Init(hw.seed.GetPin(Hothouse::LED_2), false, hw.AudioCallbackRate());
    usb.Init(daisy::UsbHandle::FS_INTERNAL);
    usb.SetReceiveCallback(Receive, daisy::UsbHandle::FS_INTERNAL);
    hw.StartAdc();
    hw.StartAudio(Audio);
    unsigned last_log = 0, watch_ms = 2000, next_id = 1;
    auto message = [](const char* text) { output.Push(text, std::strlen(text)); };
    auto report = [](unsigned request = 0) {
        char text[UsbOutput::kBytes];
        const auto n = FormatSnapshot(text, sizeof(text), snapshots.Read(), request);
        if(n) output.Push(text, n);
    };
    while(true) {
        if(boot_requested.load(std::memory_order_relaxed)) {
            hw.StopAudio(); hw.StopAdc();
            for(int n = 0; n < 6; ++n) {
                leds[0].Set(n % 2); leds[1].Set((n + 1) % 2);
                for(auto& led : leds) { for(int j = 0; j < 100; ++j) led.Update(); }
                daisy::System::Delay(100);
            }
            // Persistent DFU leaves time to start a flasher and keeps the
            // installed Daisy bootloader intact.
            daisy::System::ResetToBootloader(daisy::System::BootloaderMode::DAISY_INFINITE_TIMEOUT);
        }
        const unsigned now = daisy::System::GetNow();
        // Parse in main only, with bounded work and output backpressure.
        for(unsigned n = 0; n < 8 && output.Free() >= 2; ++n) {
            char line[128]; const auto received = receiver.Next(line, sizeof(line));
            if(received == CommandReceiver::Result::None) break;
            if(received != CommandReceiver::Result::Line) {
                message(received == CommandReceiver::Result::Overflow
                    ? "{\"type\":\"error\",\"error\":\"rx_overflow\"}\r\n"
                    : "{\"type\":\"error\",\"error\":\"line_too_long\"}\r\n");
                continue;
            }
            if(!line[0]) continue;
            Command c; const char* error = nullptr;
            if(!ParseCommand(line, c, error)) {
                char text[128]; std::snprintf(text,sizeof(text),"{\"type\":\"error\",\"error\":\"%s\"}\r\n",error);
                message(text); continue;
            }
            if(c.operation == Operation::Get) { report(c.value); continue; }
            if(c.operation == Operation::Ping) { message("{\"type\":\"pong\"}\r\n"); continue; }
            if(c.operation == Operation::Dfu) { boot_requested.store(true); continue; }
            if(c.operation == Operation::Watch) {
                watch_ms = c.value; message("{\"type\":\"ack\",\"watch\":true}\r\n"); continue;
            }
            c.id = next_id;
            if(!commands.Push(c)) { message("{\"type\":\"error\",\"error\":\"command_queue_full\"}\r\n"); continue; }
            ++next_id;
            char text[96]; std::snprintf(text,sizeof(text),"{\"type\":\"ack\",\"id\":%u,\"queued\":true}\r\n",c.id);
            message(text);
        }
        if(watch_ms && now - last_log >= watch_ms && output.Free() >= 2) {
            last_log = now;
            report();
        }
        output.Poll([](uint8_t* data, size_t bytes) { return usb.TransmitInternal(data, bytes) == daisy::UsbHandle::Result::OK; });
        daisy::System::Delay(1);
    }
}
