// =============================================================================
// videoRecExample - Native video recording sample
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(800, 600);
    settings.setTitle("videoRecExample - TrussC");

    return TC_RUN_APP(tcApp, settings);
}
