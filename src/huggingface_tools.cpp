#include "github_research/huggingface_tools.hpp"
#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <string>

namespace github_research {

namespace {

// HF 工具统一日志前缀
constexpr const char* kLogPrefix = "[hf]";

// 默认提取数量上限(保留用于参数校验)
constexpr int kDefaultCount = 10;
constexpr int kMaxCount = 50;

// 设计理念:工具只负责"取到页面内容",解析交给 AI
// 所有工具统一使用 webview_helpers.hpp 中的 kJsExtractRawPage

} // anonymous namespace

// ============================================================
// 1. ToolHfSearchModels
// ============================================================
json ToolHfSearchModels(WebViewSession& session, const json& args) {
    std::string query;
    std::string task;
    bool hasTask = false;
    int count = kDefaultCount;

    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();
    if (args.contains("task") && args["task"].is_string()) {
        task = args["task"].get<std::string>();
        hasTask = !task.empty();
    }
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (query.empty()) {
        return McpError("'query' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > kMaxCount) count = kMaxCount;

    std::string encoded = UrlEncodeComponent(query);
    std::string url = "https://huggingface.co/models?search=" + encoded;
    if (hasTask) {
        url += "&pipeline_tag=" + UrlEncodeComponent(task);
    }

    (void)count;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

// ============================================================
// 2. ToolHfGetModelInfo
// ============================================================
json ToolHfGetModelInfo(WebViewSession& session, const json& args) {
    std::string modelId;
    if (args.contains("model_id") && args["model_id"].is_string())
        modelId = args["model_id"].get<std::string>();

    if (modelId.empty()) {
        return McpError("'model_id' parameter is required");
    }
    // 清洗:去掉前导斜杠
    if (!modelId.empty() && modelId[0] == '/') modelId = modelId.substr(1);

    std::string url = "https://huggingface.co/" + modelId;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

// ============================================================
// 3. ToolHfGetModelReadme
// ============================================================
json ToolHfGetModelReadme(WebViewSession& session, const json& args) {
    std::string modelId;
    if (args.contains("model_id") && args["model_id"].is_string())
        modelId = args["model_id"].get<std::string>();

    if (modelId.empty()) {
        return McpError("'model_id' parameter is required");
    }
    if (!modelId.empty() && modelId[0] == '/') modelId = modelId.substr(1);

    std::string url = "https://huggingface.co/" + modelId;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

// ============================================================
// 4. ToolHfSearchDatasets
// ============================================================
json ToolHfSearchDatasets(WebViewSession& session, const json& args) {
    std::string query;
    int count = kDefaultCount;

    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (query.empty()) {
        return McpError("'query' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > kMaxCount) count = kMaxCount;

    std::string encoded = UrlEncodeComponent(query);
    std::string url = "https://huggingface.co/datasets?search=" + encoded;

    (void)count;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

// ============================================================
// 5. ToolHfGetDatasetInfo
// ============================================================
json ToolHfGetDatasetInfo(WebViewSession& session, const json& args) {
    std::string datasetId;
    if (args.contains("dataset_id") && args["dataset_id"].is_string())
        datasetId = args["dataset_id"].get<std::string>();

    if (datasetId.empty()) {
        return McpError("'dataset_id' parameter is required");
    }
    if (!datasetId.empty() && datasetId[0] == '/') datasetId = datasetId.substr(1);

    std::string url = "https://huggingface.co/datasets/" + datasetId;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

// ============================================================
// 6. ToolHfGetTrendingModels
// ============================================================
json ToolHfGetTrendingModels(WebViewSession& session, const json& args) {
    int count = kDefaultCount;
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (count < 1) count = 1;
    if (count > kMaxCount) count = kMaxCount;

    std::string url = "https://huggingface.co/models?sort=trending";

    (void)count;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

// ============================================================
// 7. ToolHfSearchSpaces
// ============================================================
json ToolHfSearchSpaces(WebViewSession& session, const json& args) {
    std::string query;
    int count = kDefaultCount;

    if (args.contains("query") && args["query"].is_string())
        query = args["query"].get<std::string>();
    if (args.contains("count") && args["count"].is_number_integer())
        count = args["count"].get<int>();

    if (query.empty()) {
        return McpError("'query' parameter is required");
    }
    if (count < 1) count = 1;
    if (count > kMaxCount) count = kMaxCount;

    std::string encoded = UrlEncodeComponent(query);
    std::string url = "https://huggingface.co/spaces?search=" + encoded;

    (void)count;
    return NavigateAndExecute(session, to_wstring(url), kJsExtractRawPage,
                              kLogPrefix, 2500, 30000);
}

} // namespace github_research
