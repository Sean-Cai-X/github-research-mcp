#include "github_research/github_client.hpp"
#include "github_research/string_utils.hpp"
#include "github_research/formatters.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace github_research {

// === 构造 ===

GitHubClient::GitHubClient(std::optional<std::string> token, int timeout_seconds)
    : http_client_("Deep-Research-Bot/1.0", timeout_seconds, true),
      timeout_seconds_(timeout_seconds) {
    headers_["Accept"] = "application/vnd.github.v3+json";
    headers_["User-Agent"] = "Deep-Research-Bot/1.0";
    if (token && !token->empty()) {
        headers_["Authorization"] = "token " + *token;
    }
}

// === URL 拼接 ===

std::string GitHubClient::build_url(const std::string& endpoint,
                                    const std::map<std::string, std::string>& params) {
    std::string url = "https://api.github.com" + endpoint;
    if (!params.empty()) {
        url += "?" + build_query(params);
    }
    return url;
}

// === 统一 GET(返回 JSON) ===

json GitHubClient::http_get(const std::string& endpoint,
                            const std::map<std::string, std::string>& params,
                            const std::optional<std::string>& accept) {
    std::string url = build_url(endpoint, params);
    std::map<std::string, std::string> hdrs = headers_;
    if (accept) {
        hdrs["Accept"] = *accept;
    }

    // 首次请求时延迟初始化 HTTP 后端(WebView2 优先,失败 fallback WinHTTP)
    if (!ensure_ready()) {
        throw GitHubAPIError("HTTP backend initialization failed (both WebView2 and WinHTTP unavailable)",
                             0, url, "");
    }

    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> resp_headers;

    if (use_winhttp_fallback_) {
        // WinHTTP 后端
        WinHttpResponse resp = winhttp_client_->get(url, hdrs);
        status_code = resp.status_code;
        body = resp.body;
        resp_headers = resp.headers;
    } else {
        // WebView2 后端
        HttpResponse resp = http_client_.get(url, hdrs);
        status_code = resp.status_code;
        body = resp.body;
        resp_headers = resp.headers;
    }

    // 网络层错误(status_code == 0)
    if (status_code == 0) {
        throw GitHubAPIError(body, 0, url, "");
    }

    // 限流检查:403/429 + X-RateLimit-Remaining: 0
    if (status_code == 403 || status_code == 429) {
        auto it = resp_headers.find("x-ratelimit-remaining");
        if (it != resp_headers.end() && it->second == "0") {
            auto reset_it = resp_headers.find("x-ratelimit-reset");
            std::string reset_at = (reset_it != resp_headers.end()) ? reset_it->second : "";
            throw GitHubRateLimitError("rate limit exceeded", reset_at);
        }
    }

    // HTTP 错误
    if (status_code >= 400) {
        std::string body_preview = body.substr(0, std::min<size_t>(body.size(), 500));
        std::string msg = "HTTP " + std::to_string(status_code);
        if (status_code == 404) msg = "repository not found";
        throw GitHubAPIError(msg, status_code, url, body_preview);
    }

    // 解析 JSON
    try {
        return json::parse(body);
    } catch (const std::exception& e) {
        throw GitHubAPIError(std::string("invalid JSON: ") + e.what(),
                             status_code, url,
                             body.substr(0, std::min<size_t>(body.size(), 500)));
    }
}

// === 统一 GET(返回文本,用于 readme/file) ===

std::string GitHubClient::http_get_text(const std::string& endpoint,
                                        const std::optional<std::string>& accept) {
    std::string url = build_url(endpoint);
    std::map<std::string, std::string> hdrs = headers_;
    if (accept) {
        hdrs["Accept"] = *accept;
    }

    // 首次请求时延迟初始化 HTTP 后端
    if (!ensure_ready()) {
        throw GitHubAPIError("HTTP backend initialization failed",
                             0, url, "");
    }

    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> resp_headers;

    if (use_winhttp_fallback_) {
        WinHttpResponse resp = winhttp_client_->get(url, hdrs);
        status_code = resp.status_code;
        body = resp.body;
        resp_headers = resp.headers;
    } else {
        HttpResponse resp = http_client_.get(url, hdrs);
        status_code = resp.status_code;
        body = resp.body;
        resp_headers = resp.headers;
    }

    if (status_code == 0) {
        throw GitHubAPIError(body, 0, url, "");
    }

    if (status_code == 403 || status_code == 429) {
        auto it = resp_headers.find("x-ratelimit-remaining");
        if (it != resp_headers.end() && it->second == "0") {
            auto reset_it = resp_headers.find("x-ratelimit-reset");
            std::string reset_at = (reset_it != resp_headers.end()) ? reset_it->second : "";
            throw GitHubRateLimitError("rate limit exceeded", reset_at);
        }
    }

    if (status_code >= 400) {
        std::string body_preview = body.substr(0, std::min<size_t>(body.size(), 500));
        std::string msg = "HTTP " + std::to_string(status_code);
        if (status_code == 404) msg = "not found";
        throw GitHubAPIError(msg, status_code, url, body_preview);
    }

    return body;
}

// === 10 个 API 方法 ===

json GitHubClient::get_repo_info(const std::string& owner, const std::string& repo) {
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo));
}

std::string GitHubClient::get_readme(const std::string& owner, const std::string& repo) {
    try {
        return http_get_text("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/readme",
                             "application/vnd.github.raw");
    } catch (const GitHubAPIError& e) {
        return "[README not found: " + std::string(e.what()) + "]";
    }
}

std::string GitHubClient::get_tree(const std::string& owner, const std::string& repo,
                                   const std::string& branch, int max_depth, bool recursive) {
    std::map<std::string, std::string> params;
    if (recursive) params["recursive"] = "1";

    json tree_data;
    try {
        tree_data = http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) +
                             "/git/trees/" + url_encode(branch), params);
    } catch (const GitHubAPIError&) {
        // main 失败时回退 master(仅当 branch == "main")
        if (branch == "main") {
            tree_data = http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) +
                                 "/git/trees/master", params);
        } else {
            throw;
        }
    }

    return format_tree(tree_data, max_depth);
}

std::string GitHubClient::get_file_content(const std::string& owner, const std::string& repo,
                                           const std::string& path) {
    try {
        return http_get_text("/repos/" + url_encode(owner) + "/" + url_encode(repo) +
                             "/contents/" + url_encode(path),
                             "application/vnd.github.raw");
    } catch (const GitHubAPIError& e) {
        return "[File not found: " + std::string(e.what()) + "]";
    }
}

json GitHubClient::get_languages(const std::string& owner, const std::string& repo) {
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/languages");
}

json GitHubClient::get_contributors(const std::string& owner, const std::string& repo, int limit) {
    int per_page = std::min(limit, 100);
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/contributors",
                    {{"per_page", std::to_string(per_page)}});
}

json GitHubClient::get_recent_commits(const std::string& owner, const std::string& repo,
                                      int limit, const std::optional<std::string>& since) {
    std::map<std::string, std::string> params;
    params["per_page"] = std::to_string(std::min(limit, 100));
    if (since) params["since"] = *since;
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/commits", params);
}

json GitHubClient::get_issues(const std::string& owner, const std::string& repo,
                              const std::string& state, int limit,
                              const std::optional<std::string>& labels) {
    std::map<std::string, std::string> params;
    params["state"] = state;
    params["per_page"] = std::to_string(std::min(limit, 100));
    if (labels) params["labels"] = *labels;
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/issues", params);
}

json GitHubClient::get_pull_requests(const std::string& owner, const std::string& repo,
                                     const std::string& state, int limit) {
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/pulls",
                    {{"state", state}, {"per_page", std::to_string(std::min(limit, 100))}});
}

json GitHubClient::get_releases(const std::string& owner, const std::string& repo, int limit) {
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/releases",
                    {{"per_page", std::to_string(std::min(limit, 100))}});
}

json GitHubClient::get_tags(const std::string& owner, const std::string& repo, int limit) {
    return http_get("/repos/" + url_encode(owner) + "/" + url_encode(repo) + "/tags",
                    {{"per_page", std::to_string(std::min(limit, 100))}});
}

json GitHubClient::search_issues(const std::string& owner, const std::string& repo,
                                 const std::string& query, int limit) {
    std::string q = "repo:" + owner + "/" + repo + " " + query;
    return http_get("/search/issues",
                    {{"q", q}, {"per_page", std::to_string(std::min(limit, 100))}});
}

// === summarize_repo ===

json GitHubClient::summarize_repo(const std::string& owner, const std::string& repo) {
    json info = get_repo_info(owner, repo);

    json summary = json::object();
    summary["name"] = info.value("full_name", nullptr);
    summary["description"] = info.value("description", nullptr);
    summary["url"] = info.value("html_url", nullptr);
    summary["stars"] = info.value("stargazers_count", 0);
    summary["forks"] = info.value("forks_count", 0);
    summary["open_issues"] = info.value("open_issues_count", 0);
    summary["language"] = info.value("language", nullptr);
    if (info.contains("license") && info["license"].is_object()) {
        summary["license"] = info["license"].value("spdx_id", nullptr);
    } else {
        summary["license"] = nullptr;
    }
    summary["created_at"] = info.value("created_at", nullptr);
    summary["updated_at"] = info.value("updated_at", nullptr);
    summary["pushed_at"] = info.value("pushed_at", nullptr);
    summary["default_branch"] = info.value("default_branch", nullptr);
    if (info.contains("topics") && info["topics"].is_array()) {
        summary["topics"] = info["topics"];
    } else {
        summary["topics"] = json::array();
    }

    // languages
    try {
        summary["languages"] = get_languages(owner, repo);
    } catch (...) {
        summary["languages"] = json::object();
    }

    // contributor_count(保留 Python 版语义:调用两次,实际只用第二次的 size)
    try {
        // 第一次调用(limit=1)的结果未被使用,与 Python 版一致
        // 这里省略以减少无效请求,直接调用 limit=100
        json contributors = get_contributors(owner, repo, 100);
        summary["contributor_count"] = contributors.is_array() ? static_cast<int>(contributors.size()) : 0;
    } catch (...) {
        summary["contributor_count"] = "N/A";
    }

    // latest_release
    try {
        json releases = get_releases(owner, repo, 1);
        if (releases.is_array() && !releases.empty()) {
            json r = releases[0];
            summary["latest_release"] = {
                {"tag", r.value("tag_name", nullptr)},
                {"name", r.value("name", nullptr)},
                {"date", r.value("published_at", nullptr)}
            };
        } else {
            summary["latest_release"] = nullptr;
        }
    } catch (...) {
        summary["latest_release"] = nullptr;
    }

    return summary;
}

} // namespace github_research
