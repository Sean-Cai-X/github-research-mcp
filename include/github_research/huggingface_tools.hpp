#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "webview_session.hpp"

namespace github_research {

using json = nlohmann::json;

json ToolHfSearchModels(WebViewSession& session, const json& args);
json ToolHfGetModelInfo(WebViewSession& session, const json& args);
json ToolHfGetModelReadme(WebViewSession& session, const json& args);
json ToolHfSearchDatasets(WebViewSession& session, const json& args);
json ToolHfGetDatasetInfo(WebViewSession& session, const json& args);
json ToolHfGetTrendingModels(WebViewSession& session, const json& args);
json ToolHfSearchSpaces(WebViewSession& session, const json& args);

// 分层工具: 缓存 + entity_mapper
//   - ToolHfFetchModelDetail:    cache_key=hf:model:{id},       TTL=12h, entity=model + derived_from 关系
//   - ToolHfFetchDatasetDetail:  cache_key=hf:dataset:{id},     TTL=24h, entity=dataset
json ToolHfFetchModelDetail(WebViewSession& session, const json& args);
json ToolHfFetchDatasetDetail(WebViewSession& session, const json& args);

} // namespace github_research
