#include "github_research/hackernews_tools.hpp"
#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <string>

namespace github_research {

// ============================================================
// 内置 JS 脚本
// ============================================================
// 设计理念:工具只负责"取到页面内容",解析交给 AI
// 所有工具统一使用 webview_helpers.hpp 中的 kJsExtractRawPage

// ============================================================
// 工具实现
// ============================================================

// 1. hn_get_topstories
json ToolHnGetTopStories(WebViewSession& session, const json& args) {
    int count = 20;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 100) count = 100;

    std::wstring url = L"https://news.ycombinator.com/";
    json result = NavigateAndExecute(session, url, kJsExtractRawPage, "[hn]", 2000, 30000);

    // 如果请求的 count 小于返回数量,在 text 中截断
    // 此处保持简单:整页返回,count 仅作参考参数
    (void)count;
    return result;
}

// 2. hn_get_new_stories
json ToolHnGetNewStories(WebViewSession& session, const json& args) {
    int count = 20;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 100) count = 100;

    std::wstring url = L"https://news.ycombinator.com/newest";
    json result = NavigateAndExecute(session, url, kJsExtractRawPage, "[hn]", 2000, 30000);

    (void)count;
    return result;
}

// 3. hn_get_best_stories
json ToolHnGetBestStories(WebViewSession& session, const json& args) {
    int count = 20;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 100) count = 100;

    std::wstring url = L"https://news.ycombinator.com/best";
    json result = NavigateAndExecute(session, url, kJsExtractRawPage, "[hn]", 2000, 30000);

    (void)count;
    return result;
}

// 4. hn_get_item
json ToolHnGetItem(WebViewSession& session, const json& args) {
    int id = 0;
    if (args.contains("id") && args["id"].is_number_integer()) {
        id = args["id"].get<int>();
    } else if (args.contains("id") && args["id"].is_string()) {
        try {
            id = std::stoi(args["id"].get<std::string>());
        } catch (...) {
            id = 0;
        }
    }

    if (id <= 0) {
        return McpError("ERROR: [hn] 'id' parameter is required and must be a positive integer");
    }

    std::string urlStr = "https://news.ycombinator.com/item?id=" + std::to_string(id);
    std::wstring url = to_wstring(urlStr);

    return NavigateAndExecute(session, url, kJsExtractRawPage, "[hn]", 2500, 30000);
}

// 5. hn_search_by_keyword
json ToolHnSearchByKeyword(WebViewSession& session, const json& args) {
    std::string query;
    int count = 10;

    if (args.contains("query") && args["query"].is_string()) {
        query = args["query"].get<std::string>();
    }
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }

    if (query.empty()) {
        return McpError("ERROR: [hn] 'query' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > 50) count = 50;

    std::string encoded = UrlEncodeComponent(query);
    std::string urlStr = "https://hn.algolia.com/?q=" + encoded;
    std::wstring url = to_wstring(urlStr);

    // Algolia 是 React SPA,等待时间稍长
    json result = NavigateAndExecute(session, url, kJsExtractRawPage, "[hn]", 3000, 30000);

    (void)count;
    return result;
}

} // namespace github_research
