// =============================================================================
// Windows system font lookup — stub (DirectWrite impl planned)
// =============================================================================

#include "tc/utils/tcSystemFont.h"

#if defined(_WIN32)

namespace trussc {

// TODO: DirectWrite backend
//   DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, ...) → IDWriteFactory
//   factory->GetSystemFontCollection(&coll)
//   coll->FindFamilyName(L"Yu Gothic", &index, &exists)
//   family->GetFont(0, &font) → font->CreateFontFace(&face)
//   face->GetFiles(...) → IDWriteFontFile → GetReferenceKey / GetLocalFileLoader
//   for path. Need to add `dwrite` to target_link_libraries.

std::string systemFontPath(const std::string& /*name*/) {
    return "";
}

std::vector<std::string> listSystemFonts() {
    return {};
}

} // namespace trussc

#endif // _WIN32
