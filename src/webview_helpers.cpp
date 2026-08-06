#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>

namespace github_research {

json McpError(const std::string& msg) {
    return {
        {"content", json::array({{{"type", "text"}, {"text", msg}}})},
        {"isError", true}
    };
}

json McpSuccess(const json& payload) {
    return {
        {"content", json::array({{{"type", "text"}, {"text", payload.dump()}}})},
        {"isError", false}
    };
}

json WrapMcpResult(const json& payload) {
    bool isError = false;
    if (payload.is_object()) {
        // success=false 或有 error 字段时标记 isError
        if (payload.contains("success") && payload["success"].is_boolean()) {
            isError = !payload["success"].get<bool>();
        } else if (payload.contains("error") && payload["error"].is_string()) {
            isError = true;
        }
    }
    return {
        {"content", json::array({{{"type", "text"}, {"text", payload.dump()}}})},
        {"isError", isError}
    };
}

json NavigateAndExecute(WebViewSession& session,
                        const std::wstring& url,
                        const std::string& js,
                        const char* logPrefix,
                        int waitMs,
                        uint32_t navTimeoutMs) {
    if (!session.IsReady()) {
        return McpError(std::string("ERROR: ") + logPrefix +
                        " WebView session not initialized.");
    }

    // 1. 导航
    HRESULT hr = session.Navigate(url);
    if (FAILED(hr)) {
        std::cerr << logPrefix << " Navigate failed: 0x" << std::hex << hr << std::endl;
        return McpError(std::string("ERROR: ") + logPrefix + " navigation failed");
    }

    // 2. 等待 NavigationCompleted
    HRESULT navRes = session.WaitForNavigation(navTimeoutMs);
    if (FAILED(navRes)) {
        std::cerr << logPrefix << " Nav timeout, still attempt read" << std::endl;
    } else {
        // 额外等待 DOM 渲染稳定
        if (waitMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        }
    }

    // 3. 执行 JS
    ScriptResult sr = session.ExecuteScript(js);
    if (!sr.success) {
        std::cerr << logPrefix << " JS exec failed: " << sr.error << std::endl;
        return McpError(std::string("ERROR: ") + logPrefix + " JS exec failed: " + sr.error);
    }

    // 4. 解析 JSON
    try {
        json parsed = json::parse(sr.data);
        // WebView2 ExecuteScript 返回值可能是 JSON 转义字符串,再解析一次
        if (parsed.is_string()) {
            parsed = json::parse(parsed.get<std::string>());
        }
        return WrapMcpResult(parsed);
    } catch (const std::exception& e) {
        std::string raw = sr.data.substr(0, 300);
        std::cerr << logPrefix << " JSON parse failed: " << e.what()
                  << " raw=" << raw << std::endl;
        return McpError(std::string("ERROR: ") + logPrefix +
                        " result parse failed: " + e.what() + " raw=" + raw);
    }
}

// 与 NavigateAndExecute 相同流程,但返回原始 JSON payload(不包装 MCP content)
// 失败时返回 null json
json NavigateAndExecuteRaw(WebViewSession& session,
                           const std::wstring& url,
                           const std::string& js,
                           const char* logPrefix,
                           int waitMs,
                           uint32_t navTimeoutMs) {
    if (!session.IsReady()) {
        std::cerr << logPrefix << " session not ready" << std::endl;
        return nullptr;
    }

    HRESULT hr = session.Navigate(url);
    if (FAILED(hr)) {
        std::cerr << logPrefix << " Navigate failed: 0x" << std::hex << hr << std::endl;
        return nullptr;
    }

    HRESULT navRes = session.WaitForNavigation(navTimeoutMs);
    if (FAILED(navRes)) {
        std::cerr << logPrefix << " Nav timeout, still attempt read" << std::endl;
    } else {
        if (waitMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(waitMs));
        }
    }

    ScriptResult sr = session.ExecuteScript(js);
    if (!sr.success) {
        std::cerr << logPrefix << " JS exec failed: " << sr.error << std::endl;
        return nullptr;
    }

    // 第一次尝试直接解析
    try {
        json parsed = json::parse(sr.data);
        if (parsed.is_string()) {
            parsed = json::parse(parsed.get<std::string>());
        }
        return parsed;
    } catch (const std::exception& e) {
        // 第一次解析失败(常见原因:ar5iv 等站点返回含特殊 Unicode 字符,如 en-dash,
        // 在 WebView2 UTF-16->UTF-8 转换时产生非法 UTF-8 字节)
        // 清洗策略:去掉非 ASCII 且非合法 UTF-8 起始字节的字符,替换为 '?'
        std::string cleaned;
        cleaned.reserve(sr.data.size());
        size_t i = 0;
        while (i < sr.data.size()) {
            unsigned char c = static_cast<unsigned char>(sr.data[i]);
            if (c < 0x80) {
                // ASCII,直接保留
                cleaned.push_back(static_cast<char>(c));
                ++i;
            } else if (c >= 0xC2 && c <= 0xDF && i + 1 < sr.data.size()) {
                // 2 字节 UTF-8: 110xxxxx 10xxxxxx
                unsigned char c2 = static_cast<unsigned char>(sr.data[i+1]);
                if (c2 >= 0x80 && c2 <= 0xBF) {
                    cleaned.push_back(static_cast<char>(c));
                    cleaned.push_back(static_cast<char>(c2));
                    i += 2;
                } else {
                    cleaned.push_back('?');
                    ++i;
                }
            } else if (c >= 0xE0 && c <= 0xEF && i + 2 < sr.data.size()) {
                // 3 字节 UTF-8
                unsigned char c2 = static_cast<unsigned char>(sr.data[i+1]);
                unsigned char c3 = static_cast<unsigned char>(sr.data[i+2]);
                if (c2 >= 0x80 && c2 <= 0xBF && c3 >= 0x80 && c3 <= 0xBF) {
                    cleaned.push_back(static_cast<char>(c));
                    cleaned.push_back(static_cast<char>(c2));
                    cleaned.push_back(static_cast<char>(c3));
                    i += 3;
                } else {
                    cleaned.push_back('?');
                    ++i;
                }
            } else if (c >= 0xF0 && c <= 0xF4 && i + 3 < sr.data.size()) {
                // 4 字节 UTF-8
                unsigned char c2 = static_cast<unsigned char>(sr.data[i+1]);
                unsigned char c3 = static_cast<unsigned char>(sr.data[i+2]);
                unsigned char c4 = static_cast<unsigned char>(sr.data[i+3]);
                if (c2 >= 0x80 && c2 <= 0xBF && c3 >= 0x80 && c3 <= 0xBF && c4 >= 0x80 && c4 <= 0xBF) {
                    cleaned.push_back(static_cast<char>(c));
                    cleaned.push_back(static_cast<char>(c2));
                    cleaned.push_back(static_cast<char>(c3));
                    cleaned.push_back(static_cast<char>(c4));
                    i += 4;
                } else {
                    cleaned.push_back('?');
                    ++i;
                }
            } else {
                // 非法 UTF-8 起始字节,替换为 '?'
                cleaned.push_back('?');
                ++i;
            }
        }

        // 用清洗后的字符串重试
        try {
            json parsed = json::parse(cleaned);
            if (parsed.is_string()) {
                parsed = json::parse(parsed.get<std::string>());
            }
            std::cerr << logPrefix << " JSON parse recovered after UTF-8 cleanup" << std::endl;
            return parsed;
        } catch (const std::exception& e2) {
            std::string raw = sr.data.substr(0, 300);
            std::cerr << logPrefix << " JSON parse failed even after cleanup: " << e2.what()
                      << " raw=" << raw << std::endl;
            return nullptr;
        }
    }
}

std::string UrlEncodeComponent(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << static_cast<char>(c);
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(c);
        }
    }
    return escaped.str();
}

} // namespace github_research
