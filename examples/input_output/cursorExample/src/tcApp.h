#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

// cursorExample - Mouse Cursor Demo
// Shows all system cursors, custom cursor from Image file, and Fbo-drawn cursor

class tcApp : public App {
public:
    void setup() override;
    void draw() override;

private:
    struct CursorEntry {
        Cursor cursor;
        string name;
    };
    vector<CursorEntry> entries;

    int hoveredIndex = -1;

    Image cursorImage;   // loaded from file
    Fbo fbo;
    Image fboImage;      // captured from Fbo
};
