#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "webview_session.hpp"

namespace github_research {

using json = nlohmann::json;

json ToolPwcSearchPapers(WebViewSession& session, const json& args);
json ToolPwcGetPaperDetail(WebViewSession& session, const json& args);
json ToolPwcGetSota(WebViewSession& session, const json& args);
json ToolPwcSearchTasks(WebViewSession& session, const json& args);
json ToolPwcSearchDatasets(WebViewSession& session, const json& args);

} // namespace github_research
