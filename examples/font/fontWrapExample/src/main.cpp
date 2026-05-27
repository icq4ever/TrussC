// =============================================================================
// main.cpp - Font line-wrap demo (horizontal + vertical)
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(1200, 820);
    settings.setTitle("fontWrapExample - TrussC");

    return TC_RUN_APP(tcApp, settings);
}
