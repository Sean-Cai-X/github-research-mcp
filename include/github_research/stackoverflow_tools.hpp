#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "webview_session.hpp"

namespace github_research {

using json = nlohmann::json;

json ToolSoSearchQuestions(WebViewSession& session, const json& args);
json ToolSoGetQuestionDetail(WebViewSession& session, const json& args);
json ToolSoGetTopAnswers(WebViewSession& session, const json& args);
json ToolSoSearchByTags(WebViewSession& session, const json& args);
json ToolSoGetSimilar(WebViewSession& session, const json& args);

} // namespace github_research
