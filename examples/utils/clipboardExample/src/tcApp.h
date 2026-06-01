#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void draw() override;
    void keyPressed(int key) override;

    // Fired on the platform paste gesture (Cmd+V / Ctrl+V / browser paste)
    void onClipboardPasted(ClipboardPastedEventArgs& e);

private:
    int selected_ = 0;  // 0=none, 1-3=preset
    vector<string> presets_ = {"Hello, World!", "TrussC Framework", "12345"};
    vector<string> pastedLines_;
    EventListener pasteListener_;
};
