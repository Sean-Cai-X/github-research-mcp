#pragma once

#include <stdexcept>
#include <string>

namespace github_research {

// GitHub API 通用错误,携带 status_code 与 url 便于客户端诊断
class GitHubAPIError : public std::runtime_error {
public:
    GitHubAPIError(const std::string& msg, int status_code, const std::string& url, const std::string& body = "")
        : std::runtime_error(msg), status_code_(status_code), url_(url), body_(body) {}

    int status_code() const { return status_code_; }
    const std::string& url() const { return url_; }
    const std::string& body() const { return body_; }

private:
    int status_code_;
    std::string url_;
    std::string body_;  // 响应体前 500 字符,用于诊断
};

// GitHub 限流错误,附带 reset_at
class GitHubRateLimitError : public GitHubAPIError {
public:
    GitHubRateLimitError(const std::string& msg, const std::string& reset_at)
        : GitHubAPIError(msg, 429, "", ""), reset_at_(reset_at) {}

    const std::string& reset_at() const { return reset_at_; }

private:
    std::string reset_at_;
};

} // namespace github_research
