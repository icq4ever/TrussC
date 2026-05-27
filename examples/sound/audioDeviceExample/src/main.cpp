// =============================================================================
// main.cpp - Audio Device Example
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(960, 600);
    settings.setTitle("audioDeviceExample - TrussC");

    return TC_RUN_APP(tcApp, settings);
}
