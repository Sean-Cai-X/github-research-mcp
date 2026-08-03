#include "github_research/string_utils.hpp"
#include <windows.h>
#include <sstream>
#include <iomanip>

namespace github_research {

std::wstring to_wstring(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                   static_cast<int>(utf8.size()),
                                   nullptr, 0);
    std::wstring wide(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                        static_cast<int>(utf8.size()),
                        wide.data(), wlen);
    return wide;
}

std::string to_utf8(const std::wstring& wide) {
    if (wide.empty()) return std::string();
    int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                   static_cast<int>(wide.size()),
                                   nullptr, 0, nullptr, nullptr);
    std::string utf8(ulen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                        static_cast<int>(wide.size()),
                        utf8.data(), ulen, nullptr, nullptr);
    return utf8;
}

std::string url_encode(const std::string& value) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            oss << static_cast<char>(c);
        } else {
            oss << '%' << std::setw(2) << std::setfill('0')
                << static_cast<int>(c);
        }
    }
    return oss.str();
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    for (auto& c : result) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return result;
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (tolower(static_cast<unsigned char>(a[i])) !=
            tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string build_query(const std::map<std::string, std::string>& params) {
    std::string q;
    bool first = true;
    for (const auto& [k, v] : params) {
        if (!first) q += '&';
        q += url_encode(k) + '=' + url_encode(v);
        first = false;
    }
    return q;
}

} // namespace github_research
