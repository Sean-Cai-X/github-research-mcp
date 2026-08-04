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

} // namespace github_research
