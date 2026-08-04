#include "github_research/arxiv_tools.hpp"
#include "github_research/webview_helpers.hpp"  // NavigateAndExecute / McpError / McpSuccess
#include "github_research/string_utils.hpp"  // to_utf8 / to_wstring
#include <iostream>
#include <sstream>
#include <iomanip>
#include <codecvt>
#include <locale>
#include <thread>
#include <chrono>

namespace github_research {

namespace {

// ============== 辅助:URL 编码 ==============
std::string UrlEncode(const std::string& str) {
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

// ============== 内置 JS 脚本 ==============
// 设计理念:工具只负责"取到页面内容",解析交给 AI
// 所有工具统一使用 webview_helpers.hpp 中的 kJsExtractRawPage

// 网站连通性检测 (https://arxiv.org 首页) - 仅此工具保留专用 JS
constexpr const char* kJsCheckAvailable = R"(
(function(){
    var hasHeader = !!document.querySelector("header#header, .header-banner, header[role=banner]");
    var hasSearch = !!document.querySelector('input[name="query"], form[action*="search"] input[type=text]');
    var hasLogo = !!document.querySelector('img[src*="arxiv"], svg, a#logo, .logo');
    var title = document.title || "";
    return JSON.stringify({
        success: true,
        available: (hasHeader || hasSearch || hasLogo || title.toLowerCase().indexOf("arxiv") !== -1),
        page_title: title
    });
})();
)";

// ============== 通用:导航 + 等待 + 执行JS ==============
// 已统一到 webview_helpers.hpp 的 NavigateAndExecute
// 所有 6 源工具共用同一调用模式(Navigate + ExecuteScript)

} // anonymous namespace

// ============================================================
// 1. arxiv_search_papers
// ============================================================
json ToolArxivSearchPapers(WebViewSession& session, const json& args) {
    std::string query;
    int maxResults = 10;

    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();
    if (args.contains("max_results") && args["max_results"].is_number_integer())
        maxResults = args["max_results"].get<int>();

    if (query.empty()) {
        return {
            {"content", json::array({{{"type", "text"}, {"text", "ERROR: 'query' parameter is required"}}})},
            {"isError", true}
        };
    }
    if (maxResults < 1) maxResults = 1;
    if (maxResults > 50) maxResults = 50;

    // 构造搜索 URL,start=0,按相关度排序
    std::string encoded = UrlEncode(query);
    std::wstring url = to_wstring(
        "https://arxiv.org/search/?query=" + encoded +
        "&searchtype=all&start=0&order=-announced_date_first");

    // 统一返回原始页面文本,解析交给 AI
    (void)maxResults;
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[arxiv]", 2500, 30000);
}

// ============================================================
// 2. arxiv_get_paper_detail
// ============================================================
json ToolArxivGetPaperDetail(WebViewSession& session, const json& args) {
    std::string arxivId;
    if (args.contains("arxiv_id") && args["arxiv_id"].is_string())
        arxivId = args["arxiv_id"].get<std::string>();

    if (arxivId.empty()) {
        return {
            {"content", json::array({{{"type", "text"}, {"text", "ERROR: 'arxiv_id' parameter is required"}}})},
            {"isError", true}
        };
    }
    // 清洗:去掉可能的 .pdf 后缀
    if (arxivId.size() > 4 &&
        arxivId.compare(arxivId.size() - 4, 4, ".pdf") == 0)
        arxivId = arxivId.substr(0, arxivId.size() - 4);

    std::wstring url = to_wstring("https://arxiv.org/abs/" + arxivId);
    // 统一返回原始页面文本,解析交给 AI
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[arxiv]", 1500, 30000);
}

// ============================================================
// 3. arxiv_get_pdf_link (无需 WebView,零延迟)
// ============================================================
json ToolArxivGetPdfLink(const json& args) {
    std::string arxivId;
    if (args.contains("arxiv_id") && args["arxiv_id"].is_string())
        arxivId = args["arxiv_id"].get<std::string>();

    if (arxivId.empty()) {
        return McpError("ERROR: 'arxiv_id' parameter is required");
    }
    if (arxivId.size() > 4 &&
        arxivId.compare(arxivId.size() - 4, 4, ".pdf") == 0)
        arxivId = arxivId.substr(0, arxivId.size() - 4);

    json payload = {
        {"success", true},
        {"arxiv_id", arxivId},
        {"pdf_url", "https://arxiv.org/pdf/" + arxivId},
        {"abs_url", "https://arxiv.org/abs/" + arxivId}
    };
    return McpSuccess(payload);
}

// ============================================================
// 4. arxiv_check_available
// ============================================================
json ToolArxivCheckAvailable(WebViewSession& session, const json& /*args*/) {
    std::wstring url = L"https://arxiv.org";
    return NavigateAndExecute(session, url, kJsCheckAvailable, "[arxiv]", 1500, 30000);
}

} // namespace github_research
