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

// 分层工具: 缓存 + entity_mapper
//   cache_key=so:question:{id}, TTL=24h, entity=question + answered_by 关系
json ToolSoFetchQuestionDetail(WebViewSession& session, const json& args);

} // namespace github_research
