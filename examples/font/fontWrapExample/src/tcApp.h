#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

// =============================================================================
// tcApp — line-wrap demo
//
// Demonstrates Font wrap configuration:
//   - enableWrap + setMaxLineLength (off by default)
//   - setLatinHyphenation (insert '-' on forced Latin mid-word break)
//   - setKinsoku(KinsokuLevel::PunctuationOnly | Standard) — CJK 行頭禁則
//   - setHangingPunctuation — let kinsoku chars hang past edge instead of
//                              starting next line
//   - Same API used for vertical writing (maxLineLength = column height)
// =============================================================================

class tcApp : public App {
public:
    void setup() override;
    void draw() override;

private:
    Font fontH;       // 20pt horizontal CJK
    Font fontHen;     // 18pt horizontal Latin
    Font fontV;       // 22pt vertical
    Font fontLabel;   // small Latin label font
};
