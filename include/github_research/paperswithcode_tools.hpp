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

// 分层工具: 缓存 + entity_mapper (cache_key=pwc:{paper_id}, TTL=72h)
// entity: paper 实体 + evaluated_on(task) 关系
json ToolPwcFetchPaperDetail(WebViewSession& session, const json& args);

} // namespace github_research
