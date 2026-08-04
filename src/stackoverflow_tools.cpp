#include "github_research/stackoverflow_tools.hpp"
#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <string>

namespace github_research {

namespace {

constexpr const char* kLogPrefix = "[so]";

// 将模板中的 __COUNT__ 占位符替换为 count
std::string InjectCount(const std::string& tpl, int count) {
    std::string s = tpl;
    const std::string ph = "__COUNT__";
    size_t pos = s.find(ph);
    if (pos != std::string::npos) {
        s.replace(pos, ph.size(), std::to_string(count));
    }
    return s;
}

// 校验 sort 取值,非法时回退为 relevance
std::string NormalizeSort(const std::string& s) {
    if (s == "relevance" || s == "newest" || s == "active" || s == "votes") {
        return s;
    }
    return "relevance";
}

// 分号分隔的 tags -> "+tag1+tag2"(每个 tag 已 URL 编码)
// 例如 "python;pandas" -> "+python+pandas"
std::string BuildTaggedPath(const std::string& tags) {
    std::string out;
    size_t start = 0;
    while (start <= tags.size()) {
        size_t sep = tags.find(';', start);
        std::string t;
        if (sep == std::string::npos) {
            t = tags.substr(start);
            start = tags.size() + 1;
        } else {
            t = tags.substr(start, sep - start);
            start = sep + 1;
        }
        size_t a = t.find_first_not_of(" \t");
        size_t b = t.find_last_not_of(" \t");
        if (a == std::string::npos) continue; // 空段跳过
        t = t.substr(a, b - a + 1);
        out += "+" + UrlEncodeComponent(t);
    }
    return out;
}

// ============== JS 脚本 ==============
// 设计理念:工具只负责"取到页面内容",解析交给 AI
// 所有工具统一使用 webview_helpers.hpp 中的 kJsExtractRawPage

} // anonymous namespace

// ============================================================
// 1. so_search_questions
// ============================================================
json ToolSoSearchQuestions(WebViewSession& session, const json& args) {
    if (!args.contains("query") || !args["query"].is_string() ||
        args["query"].get<std::string>().empty()) {
        return McpError("ERROR: 'query' parameter is required");
    }
    std::string query = args["query"].get<std::string>();

    std::string tag;
    if (args.contains("tag") && args["tag"].is_string()) {
        tag = args["tag"].get<std::string>();
    }

    int count = 10;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 50) count = 50;

    std::string sort = "relevance";
    if (args.contains("sort") && args["sort"].is_string()) {
        sort = NormalizeSort(args["sort"].get<std::string>());
    }

    std::string urlStr = "https://stackoverflow.com/search?q=" +
                         UrlEncodeComponent(query) + "&sort=" + sort;
    if (!tag.empty()) {
        urlStr += "&tagged=" + UrlEncodeComponent(tag);
    }

    std::wstring url = to_wstring(urlStr);
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, kLogPrefix, 2500);
}

// ============================================================
// 2. so_get_question_detail
// ============================================================
json ToolSoGetQuestionDetail(WebViewSession& session, const json& args) {
    if (!args.contains("question_id") || !args["question_id"].is_number_integer()) {
        return McpError("ERROR: 'question_id' parameter is required");
    }
    long long qid = args["question_id"].get<long long>();
    if (qid <= 0) {
        return McpError("ERROR: 'question_id' must be a positive integer");
    }

    std::wstring url = to_wstring(
        "https://stackoverflow.com/questions/" + std::to_string(qid));
    // 统一返回原始页面文本,解析交给 AI
    return NavigateAndExecute(session, url, kJsExtractRawPage, kLogPrefix, 2500);
}

// ============================================================
// 3. so_get_top_answers
// ============================================================
json ToolSoGetTopAnswers(WebViewSession& session, const json& args) {
    if (!args.contains("question_id") || !args["question_id"].is_number_integer()) {
        return McpError("ERROR: 'question_id' parameter is required");
    }
    long long qid = args["question_id"].get<long long>();
    if (qid <= 0) {
        return McpError("ERROR: 'question_id' must be a positive integer");
    }

    int count = 3;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 20) count = 20;

    std::wstring url = to_wstring(
        "https://stackoverflow.com/questions/" + std::to_string(qid) +
        "?answertab=votes");
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, kLogPrefix, 2500);
}

// ============================================================
// 4. so_search_by_tags
// ============================================================
json ToolSoSearchByTags(WebViewSession& session, const json& args) {
    if (!args.contains("tags") || !args["tags"].is_string() ||
        args["tags"].get<std::string>().empty()) {
        return McpError("ERROR: 'tags' parameter is required");
    }
    std::string tags = args["tags"].get<std::string>();

    int count = 10;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 50) count = 50;

    std::string taggedPath = BuildTaggedPath(tags);
    if (taggedPath.empty()) {
        return McpError("ERROR: 'tags' must contain at least one non-empty tag");
    }

    std::wstring url = to_wstring(
        "https://stackoverflow.com/questions/tagged" + taggedPath);
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, kLogPrefix, 2500);
}

// ============================================================
// 5. so_get_similar
// ============================================================
json ToolSoGetSimilar(WebViewSession& session, const json& args) {
    if (!args.contains("title") || !args["title"].is_string() ||
        args["title"].get<std::string>().empty()) {
        return McpError("ERROR: 'title' parameter is required");
    }
    std::string title = args["title"].get<std::string>();

    int count = 5;
    if (args.contains("count") && args["count"].is_number_integer()) {
        count = args["count"].get<int>();
    }
    if (count < 1) count = 1;
    if (count > 30) count = 30;

    std::wstring url = to_wstring(
        "https://stackoverflow.com/search?q=" + UrlEncodeComponent(title) +
        "&sort=relevance");
    // 统一返回原始页面文本,解析交给 AI
    (void)count;
    return NavigateAndExecute(session, url, kJsExtractRawPage, kLogPrefix, 2500);
}

} // namespace github_research
