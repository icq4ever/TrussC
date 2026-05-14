// =============================================================================
// main.cpp - Entry point for curveVariationsExample
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(1200, 760);
    settings.setTitle("curveVariationsExample - drawCurve / Path / loop / bezier loop");
    return TC_RUN_APP(tcApp, settings);
}
