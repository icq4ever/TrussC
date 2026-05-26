#pragma once

// =============================================================================
// TrussC vertical writing support (UAX #50 + CJK Compatibility Forms)
//
// Provides:
//   - WritingMode enum (Horizontal default, VerticalRL for tategaki)
//   - TcyMode enum (Rotate / Upright / Combine) for Latin & digit runs
//   - UAX #50 Vertical_Orientation lookup (range-based, no big tables)
//   - Horizontal → vertical codepoint mapping (Unicode FE10-FE4F forms)
//   - Fallback punctuation offsets for fonts missing vertical-form glyphs
// =============================================================================

#include <cstdint>

namespace trussc {

// ---------------------------------------------------------------------------
// Writing mode
// ---------------------------------------------------------------------------
enum class WritingMode {
    Horizontal,    // default — left-to-right rows. No behavioral change.
    VerticalRL,    // top-to-bottom columns, columns flow right-to-left (Japanese books)
};

// ---------------------------------------------------------------------------
// Tate-chu-yoko (縦中横) — how to render a run of Latin / digit chars
// inside vertical text.
// ---------------------------------------------------------------------------
enum class TcyMode {
    Rotate,    // Rotate the whole run 90° CW so it reads top-to-bottom
    Upright,   // Each glyph upright, one per cell (一文字ずつ正立)
    Combine,   // Squeeze the run into one CJK-sized cell (true 縦中横)
};

// ---------------------------------------------------------------------------
// UAX #50 simplified orientation classes
// ---------------------------------------------------------------------------
enum class VertOrient : uint8_t {
    U,   // Upright (CJK ideographs, kana, fullwidth forms)
    R,   // Rotated 90° CW (Latin letters/digits, halfwidth kana)
    Tu,  // Transformed Upright — vertical-form glyph available, else upright
    Tr,  // Transformed Rotated — vertical-form glyph available, else rotated
};

// ---------------------------------------------------------------------------
// Codepoint classification helpers
// ---------------------------------------------------------------------------
inline bool isAsciiDigit(uint32_t cp) {
    return cp >= '0' && cp <= '9';
}

inline bool isAsciiLetter(uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}

inline bool isCjkIdeograph(uint32_t cp) {
    return (cp >= 0x3400 && cp <= 0x4DBF)    // Ext A
        || (cp >= 0x4E00 && cp <= 0x9FFF)    // Unified
        || (cp >= 0xF900 && cp <= 0xFAFF)    // Compat
        || (cp >= 0x20000 && cp <= 0x2FFFF); // Ext B–F
}

inline bool isHiragana(uint32_t cp)  { return cp >= 0x3040 && cp <= 0x309F; }
inline bool isKatakana(uint32_t cp)  { return cp >= 0x30A0 && cp <= 0x30FF; }
inline bool isHalfwidthKana(uint32_t cp) { return cp >= 0xFF61 && cp <= 0xFF9F; }
inline bool isFullwidthLetter(uint32_t cp) {
    return (cp >= 0xFF21 && cp <= 0xFF3A) || (cp >= 0xFF41 && cp <= 0xFF5A);
}
inline bool isFullwidthDigit(uint32_t cp) { return cp >= 0xFF10 && cp <= 0xFF19; }

// ---------------------------------------------------------------------------
// UAX #50 Vertical_Orientation lookup (simplified, range-based)
// ---------------------------------------------------------------------------
inline VertOrient getVerticalOrientation(uint32_t cp) {
    // Specific characters with vertical-form variants in U+FE10–FE4F.
    // These are Tu/Tr — fall back to upright/rotated when font lacks the variant.
    switch (cp) {
        // Punctuation that should appear upright in vertical context
        case 0x002C: // ,
        case 0x002E: // .
        case 0x3001: // 、
        case 0x3002: // 。
        case 0x003A: // :
        case 0x003B: // ;
        case 0x0021: // !
        case 0x003F: // ?
        case 0x3016: // 〖
        case 0x3017: // 〗
        case 0x2026: // …
        case 0x2025: // ‥
            return VertOrient::Tu;
        // Bracket-like — Tr (default rotated, substitute available)
        case 0x2014: // —
        case 0x2013: // –
        case 0x005F: // _
        case 0x007B: // {
        case 0x007D: // }
        case 0x0028: // (
        case 0x0029: // )
        case 0xFF08: // （
        case 0xFF09: // ）
        case 0x3014: // 〔
        case 0x3015: // 〕
        case 0x3010: // 【
        case 0x3011: // 】
        case 0x300A: // 《
        case 0x300B: // 》
        case 0x3008: // 〈
        case 0x3009: // 〉
        case 0x300C: // 「
        case 0x300D: // 」
        case 0x300E: // 『
        case 0x300F: // 』
            return VertOrient::Tr;
        // Long sound marks etc. that should rotate but are already drawn as
        // long horizontal strokes — explicit Tr keeps them oriented correctly.
        case 0x30FC: // ー
            return VertOrient::Tr;
    }

    // CJK / kana / Hangul / fullwidth → upright
    if (isCjkIdeograph(cp))                          return VertOrient::U;
    if (isHiragana(cp) || isKatakana(cp))            return VertOrient::U;
    if (cp >= 0x3000 && cp <= 0x303F)                return VertOrient::U; // CJK symbols
    if (cp >= 0x3100 && cp <= 0x312F)                return VertOrient::U; // Bopomofo
    if (cp >= 0x3300 && cp <= 0x33FF)                return VertOrient::U; // CJK Compat
    if (isFullwidthLetter(cp) || isFullwidthDigit(cp)) return VertOrient::U;
    if (cp >= 0xFF00 && cp <= 0xFF5E)                return VertOrient::U; // Fullwidth ASCII
    if (cp >= 0xAC00 && cp <= 0xD7AF)                return VertOrient::U; // Hangul

    // Halfwidth katakana → rotated per UAX #50
    if (isHalfwidthKana(cp))                         return VertOrient::R;

    // CJK vertical-form codepoints themselves → upright
    if (cp >= 0xFE10 && cp <= 0xFE19)                return VertOrient::U;
    if (cp >= 0xFE30 && cp <= 0xFE4F)                return VertOrient::U;

    // Default: rotated (Latin letters/digits/symbols, Greek, Cyrillic, etc.)
    return VertOrient::R;
}

// ---------------------------------------------------------------------------
// Horizontal → vertical codepoint mapping.
// Returns 0 if no Unicode vertical-form codepoint exists for the input.
// Caller must still check whether the font actually has the resulting glyph
// (via stbtt_FindGlyphIndex) and fall back to rotation/offset otherwise.
// ---------------------------------------------------------------------------
inline uint32_t getVerticalCodepoint(uint32_t cp) {
    switch (cp) {
        // U+FE10–FE19 Vertical Forms
        case 0x002C: return 0xFE10;  // ,
        case 0x3001: return 0xFE11;  // 、
        case 0x3002: return 0xFE12;  // 。
        case 0x003A: return 0xFE13;  // :
        case 0x003B: return 0xFE14;  // ;
        case 0x0021: return 0xFE15;  // !
        case 0x003F: return 0xFE16;  // ?
        case 0x3016: return 0xFE17;  // 〖
        case 0x3017: return 0xFE18;  // 〗
        case 0x2026: return 0xFE19;  // …
        // U+FE30–FE4F CJK Compatibility Forms (vertical variants)
        case 0x2025: return 0xFE30;  // ‥
        case 0x2014: return 0xFE31;  // —
        case 0x2013: return 0xFE32;  // –
        case 0x005F: return 0xFE33;  // _
        case 0x0028: return 0xFE35;  // (
        case 0x0029: return 0xFE36;  // )
        case 0xFF08: return 0xFE35;  // （
        case 0xFF09: return 0xFE36;  // ）
        case 0x007B: return 0xFE37;  // {
        case 0x007D: return 0xFE38;  // }
        case 0x3014: return 0xFE39;  // 〔
        case 0x3015: return 0xFE3A;  // 〕
        case 0x3010: return 0xFE3B;  // 【
        case 0x3011: return 0xFE3C;  // 】
        case 0x300A: return 0xFE3D;  // 《
        case 0x300B: return 0xFE3E;  // 》
        case 0x3008: return 0xFE3F;  // 〈
        case 0x3009: return 0xFE40;  // 〉
        case 0x300C: return 0xFE41;  // 「
        case 0x300D: return 0xFE42;  // 」
        case 0x300E: return 0xFE43;  // 『
        case 0x300F: return 0xFE44;  // 』
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Fallback positional offset (in em fractions) for Tu punctuation when the
// font lacks the vertical-form glyph. Returned as fraction of em — caller
// multiplies by font size in pixels. {0,0} = no offset, draw centered.
// ---------------------------------------------------------------------------
struct VertOffset { float dx, dy; };

inline VertOffset getVerticalPunctOffset(uint32_t cp) {
    switch (cp) {
        // CJK / Western comma & period sit at upper-right of the cell.
        case 0x3001: // 、
        case 0x3002: // 。
        case 0x002C: // ,
        case 0x002E: // .
            return {0.5f, -0.5f};
    }
    return {0, 0};
}

} // namespace trussc

namespace tc = trussc;
