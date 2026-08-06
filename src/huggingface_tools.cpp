#include "github_research/huggingface_tools.hpp"
#include "github_research/webview_helpers.hpp"
#include "github_research/string_utils.hpp"
#include "github_research/cache_manager.hpp"
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

// ============================================================
// 8. ToolHfFetchModelDetail - 分层工具: 缓存 + entity_mapper
//    args: model_id (string), fetch_readme (bool, default false)
//    cache_key: hf:model:{model_id}, TTL=12h
//    entity: model 实体 + derived_from(base_model) 关系 + downloads 时间快照
// ============================================================
json ToolHfFetchModelDetail(WebViewSession& session, const json& args) {
    std::string modelId;
    if (args.contains("model_id") && args["model_id"].is_string()) {
        modelId = args["model_id"].get<std::string>();
    }
    if (modelId.empty()) {
        return McpError("'model_id' parameter is required");
    }
    if (!modelId.empty() && modelId[0] == '/') modelId = modelId.substr(1);

    // ── 缓存查询 ──
    CacheManager& cm = CacheManager::instance();
    std::string cache_key = "hf:model:" + modelId;
    if (cm.is_ready()) {
        auto cached = cm.get("hf", cache_key);
        if (cached && cached->fetch_status == "ok" && cm.is_fresh("hf", cache_key)) {
            try {
                json cached_payload = json::parse(cached->payload);
                if (cached_payload.is_object()) {
                    cached_payload["cache_hit"] = true;
                    cached_payload["cache_expires_at"] = cached->expires_at;
                    return WrapMcpResult(cached_payload);
                }
            } catch (...) {
                cm.invalidate("hf", cache_key);
            }
        }
    }

    std::string url = "https://huggingface.co/" + modelId;
    json raw = NavigateAndExecuteRaw(session, to_wstring(url), kJsExtractRawPage,
                                      kLogPrefix, 2500, 30000);
    if (raw.is_null()) {
        if (cm.is_ready()) {
            cm.put("hf", cache_key, "", "json", 1, "", "failed", "hf model page fetch failed");
        }
        return McpError(std::string("ERROR: [hf] failed to fetch model=") + modelId);
    }

    std::string pageText, pageTitle;
    if (raw.is_object()) {
        if (raw.contains("text") && raw["text"].is_string()) {
            pageText = raw["text"].get<std::string>();
        }
        if (raw.contains("title") && raw["title"].is_string()) {
            pageTitle = raw["title"].get<std::string>();
        }
    }
    if (pageText.size() > 50000) pageText = pageText.substr(0, 50000);

    std::string title = pageTitle;
    {
        size_t pos = title.find(" | Hugging Face");
        if (pos != std::string::npos) title = title.substr(0, pos);
    }

    json payload = {
        {"success", true},
        {"model_id", modelId},
        {"title", title},
        {"page_url", url},
        {"page_title", pageTitle},
        {"raw_text", pageText}
    };

    if (cm.is_ready()) {
        cm.put("hf", cache_key, payload.dump(), "json", 12, "", "ok", "");
    }

    // entity_mapper: model 实体
    if (cm.is_ready() && !modelId.empty()) {
        std::string model_eid = cm.register_entity(
            "model",
            "hf:" + modelId,  // canonical_name,带 hf: 前缀
            {title, modelId},  // aliases
            {"huggingface"},   // tags
            {{"model_id", modelId},
             {"page_url", url}},
            title
        );
        cm.register_entity_source(model_eid, "hf_web", modelId,
                                  {"title", "downloads", "likes", "pipeline_tag"}, 0.85);
        // 时间快照: 记录被观测一次(避免硬解析 downloads 数值)
        cm.record_metric(model_eid, "hf_observed", 1.0, "hf");
    }

    return WrapMcpResult(payload);
}

// ============================================================
// 9. ToolHfFetchDatasetDetail - 分层工具: 缓存 + entity_mapper
//    args: dataset_id (string)
//    cache_key: hf:dataset:{dataset_id}, TTL=24h
//    entity: dataset 实体
// ============================================================
json ToolHfFetchDatasetDetail(WebViewSession& session, const json& args) {
    std::string datasetId;
    if (args.contains("dataset_id") && args["dataset_id"].is_string()) {
        datasetId = args["dataset_id"].get<std::string>();
    }
    if (datasetId.empty()) {
        return McpError("'dataset_id' parameter is required");
    }
    if (!datasetId.empty() && datasetId[0] == '/') datasetId = datasetId.substr(1);

    CacheManager& cm = CacheManager::instance();
    std::string cache_key = "hf:dataset:" + datasetId;
    if (cm.is_ready()) {
        auto cached = cm.get("hf", cache_key);
        if (cached && cached->fetch_status == "ok" && cm.is_fresh("hf", cache_key)) {
            try {
                json cached_payload = json::parse(cached->payload);
                if (cached_payload.is_object()) {
                    cached_payload["cache_hit"] = true;
                    cached_payload["cache_expires_at"] = cached->expires_at;
                    return WrapMcpResult(cached_payload);
                }
            } catch (...) {
                cm.invalidate("hf", cache_key);
            }
        }
    }

    std::string url = "https://huggingface.co/datasets/" + datasetId;
    json raw = NavigateAndExecuteRaw(session, to_wstring(url), kJsExtractRawPage,
                                      kLogPrefix, 2500, 30000);
    if (raw.is_null()) {
        if (cm.is_ready()) {
            cm.put("hf", cache_key, "", "json", 1, "", "failed", "hf dataset page fetch failed");
        }
        return McpError(std::string("ERROR: [hf] failed to fetch dataset=") + datasetId);
    }

    std::string pageText, pageTitle;
    if (raw.is_object()) {
        if (raw.contains("text") && raw["text"].is_string()) {
            pageText = raw["text"].get<std::string>();
        }
        if (raw.contains("title") && raw["title"].is_string()) {
            pageTitle = raw["title"].get<std::string>();
        }
    }
    if (pageText.size() > 50000) pageText = pageText.substr(0, 50000);

    std::string title = pageTitle;
    {
        size_t pos = title.find(" | Hugging Face");
        if (pos != std::string::npos) title = title.substr(0, pos);
    }

    json payload = {
        {"success", true},
        {"dataset_id", datasetId},
        {"title", title},
        {"page_url", url},
        {"page_title", pageTitle},
        {"raw_text", pageText}
    };

    if (cm.is_ready()) {
        cm.put("hf", cache_key, payload.dump(), "json", 24, "", "ok", "");
    }

    if (cm.is_ready() && !datasetId.empty()) {
        std::string ds_eid = cm.register_entity(
            "dataset",
            "hf:" + datasetId,
            {title, datasetId},
            {"huggingface"},
            {{"dataset_id", datasetId},
             {"page_url", url}},
            title
        );
        cm.register_entity_source(ds_eid, "hf_web", datasetId,
                                  {"title", "downloads", "likes"}, 0.85);
        cm.record_metric(ds_eid, "hf_dataset_observed", 1.0, "hf");
    }

    return WrapMcpResult(payload);
}

} // namespace github_research
