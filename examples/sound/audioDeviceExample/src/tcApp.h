#pragma once

#include <TrussC.h>
#include <tcxImGui.h>
#include <atomic>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

    // Real-time sine synth — runs on the audio thread.
    void audioOut(AudioOutBuffer& buf) override;

private:
    // Synth parameters (touched from both UI and audio threads).
    std::atomic<float> frequency{440.0f};
    std::atomic<float> amplitude{0.3f};

    // Phase — audio-thread-only.
    double phase = 0.0;

    // Devices snapshot (refreshed periodically + after each re-init).
    std::vector<AudioDeviceInfo> devices;
    int selectedDeviceIdx = 0;       // index into `devices`
    int selectedSampleRateIdx = 0;   // index into kSampleRates

    // Available sample rates the UI lets the user pick.
    static constexpr int kSampleRates[3] = {44100, 48000, 96000};

    // Last init status (shown in UI).
    std::string lastInitMessage;
    bool lastInitOk = true;

    // Auto-stress mode: randomly switch settings every N seconds to verify
    // no combination crashes the engine.
    bool stressMode = false;
    float stressIntervalSec = 1.0f;
    float stressTimer = 0.0f;
    int   stressIterations = 0;
    int   stressFailures = 0;

    // Refresh devices list and reset selected index based on current engine.
    void refreshDevices();

    // Apply the currently-selected device + rate. Updates lastInitMessage.
    void applyCurrentSelection();
};
