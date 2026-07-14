#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace vsr {

// API and JSON strings are UTF-8. std::filesystem::path(std::string) uses the
// active ANSI code page on Windows, so cross that boundary explicitly.
inline std::filesystem::path path_from_utf8(std::string_view value) {
#if defined(__cpp_char8_t)
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const unsigned char byte : value) {
        utf8.push_back(static_cast<char8_t>(byte));
    }
    return std::filesystem::path(utf8);
#else
    return std::filesystem::u8path(value.begin(), value.end());
#endif
}

inline std::string path_to_utf8(const std::filesystem::path& value) {
    const auto utf8 = value.u8string();
    std::string result;
    result.reserve(utf8.size());
    for (const auto byte : utf8) {
        result.push_back(static_cast<char>(byte));
    }
    return result;
}

} // namespace vsr
