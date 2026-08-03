#pragma once

#include <string>
#include <optional>
#include <map>
#include <memory>
#include <iostream>
#include <nlohmann/json.hpp>
#include "webview_client.hpp"
#include "winhttp_client.hpp"
#include "errors.hpp"

namespace github_research {

using json = nlohmann::json;

// GitHub REST API 客户端
// 迁移自 DeerFlow skills/public/github-deep-research/scripts/github_api.py
// 保留所有方法语义与返回 JSON 字段名不变,仅改语言与 HTTP 后端(WebView2)
class GitHubClient {
public:
    explicit GitHubClient(std::optional<std::string> token = std::nullopt,
                          int timeout_seconds = 30);

    // === 10 个 GitHub API 方法(对应 10 个 MCP tool) ===

    // 1. github_get_repo_info
    json get_repo_info(const std::string& owner, const std::string& repo);

    // 2. github_get_readme(返回 markdown 文本)
    std::string get_readme(const std::string& owner, const std::string& repo);

    // 3. github_get_tree(返回格式化文本)
    std::string get_tree(const std::string& owner, const std::string& repo,
                         const std::string& branch = "main",
                         int max_depth = 3,
                         bool recursive = true);

    // 4. github_get_languages
    json get_languages(const std::string& owner, const std::string& repo);

    // 5. github_get_contributors
    json get_contributors(const std::string& owner, const std::string& repo, int limit = 30);

    // 6. github_get_commits
    json get_recent_commits(const std::string& owner, const std::string& repo,
                            int limit = 50,
                            const std::optional<std::string>& since = std::nullopt);

    // 7. github_get_issues
    json get_issues(const std::string& owner, const std::string& repo,
                    const std::string& state = "all", int limit = 30,
                    const std::optional<std::string>& labels = std::nullopt);

    // 8. github_get_pull_requests
    json get_pull_requests(const std::string& owner, const std::string& repo,
                           const std::string& state = "all", int limit = 30);

    // 9. github_get_releases
    json get_releases(const std::string& owner, const std::string& repo, int limit = 10);

    // 10. github_summarize_repo
    json summarize_repo(const std::string& owner, const std::string& repo);

    // === 辅助方法(非 MCP tool,但保留语义) ===

    // 获取文件内容(返回文本)
    std::string get_file_content(const std::string& owner, const std::string& repo,
                                 const std::string& path);

    // 获取 tags 列表
    json get_tags(const std::string& owner, const std::string& repo, int limit = 20);

    // 搜索 issues
    json search_issues(const std::string& owner, const std::string& repo,
                       const std::string& query, int limit = 30);

    // WebView2 是否就绪
    bool is_ready() const { return http_client_.is_ready(); }

    // 当前使用的后端名称("webview2" 或 "winhttp")
    std::string backend_name() const {
        return use_winhttp_fallback_ ? "winhttp" : "webview2";
    }

    // 首次请求前确保 HTTP 后端已就绪
    // 优先尝试 WebView2,失败则自动 fallback 到 WinHTTP
    // 返回 false 表示两个后端都不可用(极少见)
    bool ensure_ready() {
        if (use_winhttp_fallback_) return true;  // WinHTTP 无需初始化
        if (http_client_.is_ready()) return true;

        std::cerr << "[github] initializing WebView2 backend..." << std::endl;
        if (http_client_.initialize()) {
            std::cerr << "[github] WebView2 backend ready" << std::endl;
            return true;
        }

        // WebView2 失败,fallback 到 WinHTTP
        std::cerr << "[github] WARNING: WebView2 init failed, falling back to WinHTTP" << std::endl;
        std::cerr << "[github] NOTE: WinHTTP lacks browser fingerprint, anti-scraping weaker" << std::endl;
        use_winhttp_fallback_ = true;
        winhttp_client_ = std::make_unique<WinHttpClient>(
            "Deep-Research-Bot/1.0", timeout_seconds_, true);
        return true;
    }

private:
    // 统一 GET 请求封装
    // endpoint: /repos/{owner}/{repo} 等
    // params: query string 参数(未编码)
    // accept: Accept 头(覆盖默认 v3+json),用于 readme/file 走 raw
    // 返回 JSON;若响应是纯文本(readme/file),则抛 GitHubAPIError 由调用方处理
    json http_get(const std::string& endpoint,
                  const std::map<std::string, std::string>& params = {},
                  const std::optional<std::string>& accept = std::nullopt);

    // 统一 GET 请求,返回原始文本(用于 readme/file)
    std::string http_get_text(const std::string& endpoint,
                              const std::optional<std::string>& accept = std::nullopt);

    // 拼接完整 URL
    static std::string build_url(const std::string& endpoint,
                                 const std::map<std::string, std::string>& params = {});

    WebViewClient http_client_;
    std::unique_ptr<WinHttpClient> winhttp_client_;
    bool use_winhttp_fallback_ = false;
    int timeout_seconds_ = 30;
    std::map<std::string, std::string> headers_;
    std::string base_url_ = "https://api.github.com";
};

} // namespace github_research
