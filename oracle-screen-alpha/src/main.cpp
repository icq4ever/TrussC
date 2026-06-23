// DISPOSABLE render oracle for fix/screen-alpha-premult — NOT a permanent test.
// Delete this whole directory before merging (the deletion commit keeps it
// salvageable from history). See oracle-screen-alpha/README.md.
#include "tcApp.h"

int main() {
    WindowSettings settings;
    settings.setSize(880, 560);
    settings.setHighDpi(false);            // deterministic framebuffer size
    settings.setTitle("screen-alpha oracle");
    return TC_RUN_APP(tcApp, settings);
}
