#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "webview_session.hpp"

namespace github_research {

using json = nlohmann::json;

json ToolPkgSearchNpm(WebViewSession& session, const json& args);
json ToolPkgGetNpmDetail(WebViewSession& session, const json& args);
json ToolPkgSearchPypi(WebViewSession& session, const json& args);
json ToolPkgGetPypiDetail(WebViewSession& session, const json& args);

// 分层工具: 缓存 + entity_mapper (cache_key=pkg:{registry}:{name}, TTL=24h)
// entity: package 实体 + depends_on/derived_from 关系
json ToolPkgFetchDetail(WebViewSession& session, const json& args);

} // namespace github_research
