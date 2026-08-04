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
