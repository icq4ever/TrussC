#pragma once

#include <TrussC.h>
using namespace std;
using namespace tc;

// =============================================================================
// tcApp — vertical writing (tategaki) demo
//
// Demonstrates:
//   - WritingMode::VerticalRL with default rotated Latin/digit runs
//   - TcyMode variants (Rotate / Upright / Combine) for digit runs
//   - CJK punctuation & brackets using Unicode vertical-form codepoints
//     (CJK Compatibility Forms U+FE10–FE4F) when the font supplies them
// =============================================================================

class tcApp : public App {
public:
    void setup() override;
    void draw() override;

private:
    Font fontH;   // horizontal — reference / labels
    Font fontV;   // vertical, default settings (Latin = Rotate)
    Font fontVcombineDigit;  // vertical with TCY Combine for 1-2 digit runs
    Font fontVupright;       // vertical with Upright Latin/digits
    Font fontLabel;          // small label font (horizontal Latin)
};
