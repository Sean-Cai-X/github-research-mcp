#pragma once

// Hacker News MCP 工具集(5 个工具)
// 架构:通过独立 WebViewSession 访问 news.ycombinator.com 与 hn.algolia.com
// 使用 Navigate + ExecuteScript 模式,JS 注入提取 DOM
// 复用 webview_helpers 公共辅助函数,统一 MCP 返回格式

#include <string>
#include <nlohmann/json.hpp>
#include "webview_session.hpp"

namespace github_research {

using json = nlohmann::json;

// 1. hn_get_topstories - 获取 HN 首页 Top Stories
json ToolHnGetTopStories(WebViewSession& session, const json& args);

// 2. hn_get_new_stories - 获取 HN Newest 列表
json ToolHnGetNewStories(WebViewSession& session, const json& args);

// 3. hn_get_best_stories - 获取 HN Best 列表
json ToolHnGetBestStories(WebViewSession& session, const json& args);

// 4. hn_get_item - 获取单条 story 详情及评论树
json ToolHnGetItem(WebViewSession& session, const json& args);

// 5. hn_search_by_keyword - 通过 Algolia HN 搜索接口按关键字检索
json ToolHnSearchByKeyword(WebViewSession& session, const json& args);

} // namespace github_research
