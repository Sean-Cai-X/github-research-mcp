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

std::string base64_decode(const std::string& encoded) {
    static const int decode_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int d = decode_table[c];
        if (d == -1) break;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

} // namespace github_research
