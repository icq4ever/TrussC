// HotReloadExample
//
// Smoke-test target for TrussC's hot reload build path. Uses tcxImGui to
// verify that addon include directories are propagated to the guest target.
// Web/Android/iOS builds silently skip the host/guest split (per
// trussc_app.cmake), so this example also doubles as a normal example
// across every platform.

#include "tcApp.h"
#include <tcxImGui.h>
using namespace tcx;

TC_HOT_RELOAD(tcApp)

void tcApp::setup() {
    setWindowTitle("HotReloadExample");
    imguiSetup();
}

void tcApp::draw() {
    clear(0.12f);

    imguiBegin();
    ImGui::Begin("HotReload");
    ImGui::Text("Hot reload smoke test (tcxImGui addon)");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
    imguiEnd();
}

void tcApp::exit() {
    imguiShutdown();
}
