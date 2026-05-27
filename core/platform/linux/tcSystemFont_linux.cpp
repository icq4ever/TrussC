// =============================================================================
// Linux system font lookup — stub (fontconfig impl planned)
// =============================================================================

#include "tc/utils/tcSystemFont.h"

#if defined(__linux__)

namespace trussc {

// TODO: fontconfig backend
//   FcConfigGetCurrent / FcNameParse / FcConfigSubstitute / FcDefaultSubstitute
//   FcFontMatch → FcPatternGetString(FC_FILE) for systemFontPath
//   FcFontList for listSystemFonts
// Requires `pkg_check_modules(FONTCONFIG REQUIRED fontconfig)` in CMakeLists.

std::string systemFontPath(const std::string& /*name*/) {
    return "";
}

std::vector<std::string> listSystemFonts() {
    return {};
}

} // namespace trussc

#endif // __linux__
