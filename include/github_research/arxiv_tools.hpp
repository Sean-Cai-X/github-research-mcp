#pragma once

// arXiv MCP 工具集(4 个工具)
// 架构:通过独立 WebViewSession 访问 arxiv.org,JS 注入提取 DOM
// 不依赖登录态(arXiv 开放检索),保持 WebView2 统一架构一致性

#include <string>
#include <nlohmann/json.hpp>
#include "webview_session.hpp"

namespace github_research {

using json = nlohmann::json;

// ============ 工具实现(由 MCP Server dispatch 调用) ============
// session:传入 arXiv 独立 WebViewSession 引用(已 Init 并就绪)

// 1. arxiv_search_papers - 论文检索
json ToolArxivSearchPapers(WebViewSession& session, const json& args);

// 2. arxiv_get_paper_detail - 论文详情(完整摘要)
json ToolArxivGetPaperDetail(WebViewSession& session, const json& args);

// 3. arxiv_get_pdf_link - 快速获取 PDF 直链(无需访问页面)
json ToolArxivGetPdfLink(const json& args);

// 4. arxiv_check_available - 检测 arXiv 网站连通性
json ToolArxivCheckAvailable(WebViewSession& session, const json& args);

} // namespace github_research
