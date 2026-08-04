#include "github_research/paperswithcode_tools.hpp"
#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <string>

namespace github_research {

namespace {

// ============== 内置 JS 脚本 ==============
// 设计理念:工具只负责"取到页面内容",解析交给 AI
// 所有工具统一使用 webview_helpers.hpp 中的 kJsExtractRawPage

} // anonymous namespace

// ============================================================
// 1. ToolPwcSearchPapers
// args: query (string), count (int, default 10)
// ============================================================
json ToolPwcSearchPapers(WebViewSession& session, const json& args) {
    std::string query;
    int count = 10;

    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (query.empty()) {
        return McpError("ERROR: [pwc] 'query' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > 50) count = 50;

    std::string encoded = UrlEncodeComponent(query);
    std::wstring url = to_wstring("https://paperswithcode.com/search?q=" + encoded);
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pwc]", 2500);
}

// ============================================================
// 2. ToolPwcGetPaperDetail
// args: paper_id (string)
// ============================================================
json ToolPwcGetPaperDetail(WebViewSession& session, const json& args) {
    std::string paperId;
    if (args.contains("paper_id") && args["paper_id"].is_string())
        paperId = args["paper_id"].get<std::string>();

    if (paperId.empty()) {
        return McpError("ERROR: [pwc] 'paper_id' parameter is required");
    }

    std::wstring url = to_wstring("https://paperswithcode.com/paper/" + paperId);
    // 统一返回原始页面文本,解析交给 AI
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pwc]", 2500);
}

// ============================================================
// 3. ToolPwcGetSota
// args: task (string), count (int, default 20)
// ============================================================
json ToolPwcGetSota(WebViewSession& session, const json& args) {
    std::string task;
    int count = 20;

    if (args.contains("task") && args["task"].is_string())
        task = args["task"].get<std::string>();
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (task.empty()) {
        return McpError("ERROR: [pwc] 'task' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > 100) count = 100;

    // 任务 slug: 空格替换为连字符, 再 URL 编码(连字符属于 unreserved, 保留)
    std::string slug = task;
    for (char& c : slug) {
        if (c == ' ') c = '-';
    }
    slug = UrlEncodeComponent(slug);

    std::wstring url = to_wstring("https://paperswithcode.com/sota/task/" + slug);
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pwc]", 2500);
}

// ============================================================
// 4. ToolPwcSearchTasks
// args: query (string)
// ============================================================
json ToolPwcSearchTasks(WebViewSession& session, const json& args) {
    std::string query;
    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();

    if (query.empty()) {
        return McpError("ERROR: [pwc] 'query' parameter is required");
    }

    std::string encoded = UrlEncodeComponent(query);
    // 任务检索: 直接访问 /task/ENCODED(slug 形式)
    std::wstring url = to_wstring("https://paperswithcode.com/task/" + encoded);
    // 统一返回原始页面文本,解析交给 AI
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pwc]", 2500);
}

// ============================================================
// 5. ToolPwcSearchDatasets
// args: query (string), count (int, default 10)
// ============================================================
json ToolPwcSearchDatasets(WebViewSession& session, const json& args) {
    std::string query;
    int count = 10;

    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (query.empty()) {
        return McpError("ERROR: [pwc] 'query' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > 50) count = 50;

    std::string encoded = UrlEncodeComponent(query);
    std::wstring url = to_wstring("https://paperswithcode.com/datasets?q=" + encoded);
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, "[pwc]", 2500);
}

} // namespace github_research
