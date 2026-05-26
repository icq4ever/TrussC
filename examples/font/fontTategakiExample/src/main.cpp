// =============================================================================
// main.cpp - Vertical writing (tategaki) sample
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(1100, 720);
    settings.setTitle("fontTategakiExample - TrussC");

    return TC_RUN_APP(tcApp, settings);
}
