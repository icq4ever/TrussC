// =============================================================================
// audioDeviceExample - Live audio device + sample rate switching
//
// A sine wave runs continuously through App::audioOut(). The ImGui panel
// lets the user pick a playback device and sample rate; each change calls
// AudioEngine::init({...}), which tears down the device and restarts at
// the new settings. Active voices preserve their playback position across
// the swap (the sine just keeps phasing on).
//
// "Stress" mode randomly picks a different (device, rate) combo every
// `stressIntervalSec` seconds. Letting it run for a minute or two is a
// good way to confirm no combination crashes the engine.
// =============================================================================

#include "tcApp.h"
#include "TrussC.h"

constexpr int tcApp::kSampleRates[3];

void tcApp::setup() {
    // Forced 60 fps (not VSYNC) so stress-test timing is reliable even
    // when the window isn't focused.
    setFps(60);

    // Start with system defaults — gives us a known-good initial state.
    AudioEngine::getInstance().init();

    imguiSetup();
    refreshDevices();

    // For automated stress testing: launch with TC_STRESS_AUDIO=1 to start
    // randomly switching device + rate every stressIntervalSec seconds.
    if (std::getenv("TC_STRESS_AUDIO")) {
        stressMode = true;
        randomSeed((unsigned int)getUnixTime());
        logNotice("tcApp") << "TC_STRESS_AUDIO set — starting in stress mode";
    }

    logNotice("tcApp") << "Engine: "
        << AudioEngine::getInstance().getSampleRate() << " Hz, "
        << AudioEngine::getInstance().getChannels() << " ch";
    logNotice("tcApp") << "Available playback devices: " << devices.size();
    for (auto& d : devices) {
        logNotice("tcApp") << "  - " << d.name << (d.isDefault ? " (default)" : "");
    }
}

void tcApp::audioOut(AudioOutBuffer& buf) {
    const float freq = frequency.load();
    const float amp  = amplitude.load();
    const double dPhase = (TAU * (double)freq) / (double)buf.sampleRate;

    for (int i = 0; i < buf.frameCount; i++) {
        float v = amp * (float)std::sin(phase);
        for (int c = 0; c < buf.channels; c++) {
            buf.data[i * buf.channels + c] += v;
        }
        phase += dPhase;
        if (phase >= TAU) phase -= TAU;
    }
}

void tcApp::update() {
    if (!stressMode) return;

    // Use elapsed time so the timer doesn't drift on framerate hiccups.
    static float lastT = 0.0f;
    float now = getElapsedTimef();
    float dt  = now - lastT;
    lastT = now;
    stressTimer += dt;
    if (stressTimer < stressIntervalSec) return;
    stressTimer = 0.0f;

    if (devices.empty()) return;

    // Stress: only randomize the sample rate; keep the currently-selected
    // device. macOS gates some virtual playback devices behind microphone
    // permission, and switching rapidly through them blocks ma_device_init
    // / ma_device_stop on the permission infrastructure (beach ball). The
    // sample-rate path is the most invasive part of re-init anyway (it's
    // what migrates voices), so randomizing that gives the best stress
    // coverage without the OS permission landmine.
    selectedSampleRateIdx = randomInt(0, 2);

    stressIterations++;
    applyCurrentSelection();
    if (!lastInitOk) stressFailures++;
}

void tcApp::refreshDevices() {
    devices = AudioEngine::listDevices();

    // Try to keep the previously-selected device focused if it's still around.
    int defaultIdx = 0;
    for (int i = 0; i < (int)devices.size(); ++i) {
        if (devices[i].isDefault) { defaultIdx = i; break; }
    }
    if (selectedDeviceIdx < 0 || selectedDeviceIdx >= (int)devices.size()) {
        selectedDeviceIdx = defaultIdx;
    }

    // Snap the rate picker to the engine's current rate if it matches one
    // of the presets; otherwise leave the previous selection.
    int curRate = AudioEngine::getInstance().getSampleRate();
    for (int i = 0; i < 3; ++i) {
        if (kSampleRates[i] == curRate) { selectedSampleRateIdx = i; break; }
    }
}

void tcApp::applyCurrentSelection() {
    if (selectedDeviceIdx < 0 || selectedDeviceIdx >= (int)devices.size()) {
        lastInitOk = false;
        lastInitMessage = "No device selected";
        return;
    }
    AudioSettings as;
    as.sampleRate = kSampleRates[selectedSampleRateIdx];
    as.deviceName = devices[selectedDeviceIdx].name;
    bool ok = AudioEngine::getInstance().init(as);
    lastInitOk = ok;
    lastInitMessage = ok
        ? (std::string("OK: ") + as.deviceName + " @ " + std::to_string(as.sampleRate) + " Hz")
        : (std::string("FAILED: ") + as.deviceName + " @ " + std::to_string(as.sampleRate) + " Hz");

    logNotice("tcApp") << lastInitMessage;
}

void tcApp::draw() {
    clear(0.12f);

    // Plain TrussC text just so the window has something to look at when
    // ImGui is closed.
    setColor(colors::white);
    drawBitmapString(format("Engine: {} Hz, {} ch",
        AudioEngine::getInstance().getSampleRate(),
        AudioEngine::getInstance().getChannels()), 40, 40);

    setColor(0.55f);
    drawBitmapString(format("Sine: {:.0f} Hz @ amp {:.2f}",
        frequency.load(), amplitude.load()), 40, 62);

    setColor(0.4f);
    drawBitmapString(format("Stress iters: {} (failures: {})",
        stressIterations, stressFailures), 40, 84);

    // ----- ImGui panel -----
    imguiBegin();

    ImGui::SetNextWindowPos(ImVec2(10, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420, 460), ImGuiCond_FirstUseEver);
    ImGui::Begin("Audio Device Switcher");

    // Synth controls
    float freq = frequency.load();
    if (ImGui::SliderFloat("Frequency (Hz)", &freq, 20.0f, 2000.0f, "%.1f")) {
        frequency = freq;
    }
    float amp = amplitude.load();
    if (ImGui::SliderFloat("Amplitude", &amp, 0.0f, 1.0f, "%.2f")) {
        amplitude = amp;
    }

    ImGui::Separator();

    // Sample rate
    ImGui::Text("Sample Rate");
    for (int i = 0; i < 3; ++i) {
        char label[32];
        std::snprintf(label, sizeof(label), "%d Hz", kSampleRates[i]);
        if (ImGui::RadioButton(label, selectedSampleRateIdx == i)) {
            selectedSampleRateIdx = i;
            applyCurrentSelection();
        }
        if (i < 2) ImGui::SameLine();
    }

    ImGui::Separator();

    // Devices
    ImGui::Text("Playback Device");
    if (ImGui::Button("Refresh device list")) {
        refreshDevices();
    }
    ImGui::BeginChild("devices", ImVec2(0, 180), true);
    for (int i = 0; i < (int)devices.size(); ++i) {
        std::string label = devices[i].name;
        if (devices[i].isDefault) label += " (default)";
        if (ImGui::Selectable(label.c_str(), selectedDeviceIdx == i)) {
            selectedDeviceIdx = i;
            applyCurrentSelection();
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // Stress mode
    ImGui::Checkbox("Auto-stress (random switch)", &stressMode);
    ImGui::SameLine();
    ImGui::SliderFloat("interval (s)", &stressIntervalSec, 0.2f, 5.0f, "%.1f");
    ImGui::Text("Iterations: %d   Failures: %d", stressIterations, stressFailures);

    ImGui::Separator();
    ImGui::TextColored(lastInitOk ? ImVec4(0.4f, 0.9f, 0.4f, 1.0f)
                                   : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "%s", lastInitMessage.c_str());

    ImGui::End();
    imguiEnd();

    setColor(0.4f);
    drawBitmapString("FPS: " + to_string((int)getFrameRate()), 40, getWindowHeight() - 30);
}
