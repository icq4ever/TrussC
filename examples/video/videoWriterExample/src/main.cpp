// =============================================================================
// videoWriterExample - Offline frame-by-frame video export
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(800, 600);
    settings.setTitle("videoWriterExample - TrussC");

    return TC_RUN_APP(tcApp, settings);
}
