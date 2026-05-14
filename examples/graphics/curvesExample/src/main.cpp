// =============================================================================
// main.cpp - Entry point for curvesExample
// =============================================================================

#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(1100, 700);
    settings.setTitle("curvesExample - drawArc / drawBezier / drawCurve");
    return TC_RUN_APP(tcApp, settings);
}
