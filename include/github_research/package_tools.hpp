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

} // namespace github_research
