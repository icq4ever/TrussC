// DISPOSABLE oracle for fix/screen-alpha-premult. Renders Screen & Multiply
// source bars at decreasing alpha over 4 backdrops, then screenshots and quits.
//
// What CORRECT looks like (after the fix):
//   SCREEN over black : top bar (a=1) = full orange, fading smoothly to BLACK at
//                       a=0 (bottom). Before the fix every bar was full orange
//                       (alpha ignored) — that's the bug.
//   SCREEN over white : every bar stays WHITE (screen with white = white).
//   MULTIPLY over white: top bar = orange, fading smoothly to WHITE at a=0.
//   MULTIPLY over black: every bar stays BLACK.
//   The orange↔backdrop fade being SMOOTH across the 6 bars is the proof alpha
//   is now respected.
#include "tcApp.h"
#include <cstdlib>

void tcApp::draw() {
    clear(0.15f, 0.15f, 0.18f, 1.0f);

    struct BG { float r, g, b; const char* name; };
    BG bgs[4] = { {0,0,0,"black"}, {0.5f,0.5f,0.5f,"gray"}, {1,1,1,"white"}, {1,0,0,"red"} };
    float alphas[6] = { 1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.0f };
    const float sr = 1.0f, sg = 0.55f, sb = 0.0f;   // orange source

    const float colW = 200.0f, colGap = 10.0f, x0 = 30.0f;

    auto section = [&](float y0, float h, BlendMode mode, const char* title) {
        // Opaque backdrops (overwrite).
        setBlendMode(BlendMode::Disabled);
        for (int c = 0; c < 4; ++c) {
            float x = x0 + c * (colW + colGap);
            setColor(bgs[c].r, bgs[c].g, bgs[c].b, 1.0f);
            drawRect(x, y0, colW, h);
        }
        // Source bars: alpha 1.0 (top) -> 0.0 (bottom) in the chosen blend mode.
        setBlendMode(mode);
        float barH = h / 6.0f;
        for (int c = 0; c < 4; ++c) {
            float x = x0 + c * (colW + colGap);
            for (int a = 0; a < 6; ++a) {
                setColor(sr, sg, sb, alphas[a]);
                drawRect(x, y0 + a * barH, colW, barH - 2.0f);
            }
        }
        // Labels.
        setBlendMode(BlendMode::Alpha);
        setColor(1, 1, 1, 1);
        drawBitmapString(title, x0, y0 - 14.0f);
        for (int c = 0; c < 4; ++c)
            drawBitmapString(bgs[c].name, x0 + c * (colW + colGap) + 4.0f, y0 + h + 4.0f);
    };

    section( 60.0f, 180.0f, BlendMode::Screen,   "SCREEN   (src orange, alpha 1.0 top -> 0.0 bottom)");
    section(300.0f, 180.0f, BlendMode::Multiply, "MULTIPLY (src orange, alpha 1.0 top -> 0.0 bottom)");

    // Settle a few frames, then capture once and quit.
    if (++frame_ == 4) {
        const char* env = std::getenv("ORACLE_OUT");
        std::string out = env ? env : "oracle.png";
        if (saveScreenshot(out)) logNotice("oracle") << "saved " << out;
        requestExitApp();
    }
}
