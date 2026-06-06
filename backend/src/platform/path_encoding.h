#pragma once

#include <filesystem>
#include <string>

namespace vsr {

inline std::u8string bytes_to_u8string(const std::string& value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const unsigned char ch : value) {
        utf8.push_back(static_cast<char8_t>(ch));
    }
    return utf8;
}

inline std::filesystem::path path_from_utf8(const std::string& value) {
#if defined(_WIN32)
    return std::filesystem::path(bytes_to_u8string(value));
#else
    return std::filesystem::path(value);
#endif
}

inline std::string path_to_utf8(const std::filesystem::path& path) {
    const auto utf8 = path.u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

} // namespace vsr
