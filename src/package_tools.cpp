#include "github_research/package_tools.hpp"
#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <string>

namespace github_research {

namespace {

// ============== 内置 JS 脚本 ==============
// 设计理念:工具只负责"取到页面内容",解析交给 AI
// 所有工具统一使用 webview_helpers.hpp 中的 kJsExtractRawPage

// ============== 参数读取辅助 ==============
std::string read_string(const json& args, const std::string& key) {
    if (args.contains(key) && args[key].is_string()) {
        return args[key].get<std::string>();
    }
    return "";
}

int read_count(const json& args, const std::string& key, int default_val) {
    if (args.contains(key) && args[key].is_number_integer()) {
        int n = args[key].get<int>();
        if (n < 1) return 1;
        if (n > 50) return 50;
        return n;
    }
    return default_val;
}

} // anonymous namespace

// ============================================================
// 1. pkg_search_npm
// ============================================================
json ToolPkgSearchNpm(WebViewSession& session, const json& args) {
    std::string query = read_string(args, "query");
    if (query.empty()) {
        return McpError("ERROR: [pkg] 'query' parameter is required");
    }
    int count = read_count(args, "count", 10);
    (void)count;  // 统一返回原始页面文本,解析交给 AI

    std::string encoded = UrlEncodeComponent(query);
    std::wstring url = to_wstring("https://www.npmjs.com/search?q=" + encoded);
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pkg]", 2500, 30000);
}

// ============================================================
// 2. pkg_get_npm_detail
// ============================================================
json ToolPkgGetNpmDetail(WebViewSession& session, const json& args) {
    std::string name = read_string(args, "name");
    if (name.empty()) {
        return McpError("ERROR: [pkg] 'name' parameter is required");
    }

    std::wstring url = to_wstring("https://www.npmjs.com/package/" + name);
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pkg]", 2500, 30000);
}

// ============================================================
// 3. pkg_search_pypi
// ============================================================
json ToolPkgSearchPypi(WebViewSession& session, const json& args) {
    std::string query = read_string(args, "query");
    if (query.empty()) {
        return McpError("ERROR: [pkg] 'query' parameter is required");
    }
    int count = read_count(args, "count", 10);
    (void)count;  // 统一返回原始页面文本,解析交给 AI

    std::string encoded = UrlEncodeComponent(query);
    std::wstring url = to_wstring("https://pypi.org/search/?q=" + encoded);
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pkg]", 2500, 30000);
}

// ============================================================
// 4. pkg_get_pypi_detail
// ============================================================
json ToolPkgGetPypiDetail(WebViewSession& session, const json& args) {
    std::string name = read_string(args, "name");
    if (name.empty()) {
        return McpError("ERROR: [pkg] 'name' parameter is required");
    }

    std::wstring url = to_wstring("https://pypi.org/project/" + name + "/");
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pkg]", 2500, 30000);
}

} // namespace github_research
