// =============================================================================
// main.cpp - Entry point
// =============================================================================

#include "tcApp.h"

int main() {
    tc::WindowSettings settings;
    settings.setSize(960, 600);
    settings.setTitle("videoRecExample - TrussC");

    return TC_RUN_APP(tcApp, settings);
}
