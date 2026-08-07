#pragma once

#include <string>
#include <map>
#include <memory>

namespace github_research {

// HTTP 响应结构(与 WebViewClient 共用,保持二进制兼容)
struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;  // 小写键
};

// HTTP 客户端抽象接口
// 允许 GitHubClient 在 WebView2 与 libcurl 之间切换 backend
// - WebViewClient: 浏览器内核,适用于需要 JS 渲染的场景(网页源)
// - CurlHttpClient: 纯 HTTP,适用于 API 调用(GitHub REST API 返回纯 JSON)
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // 初始化后端资源(如 WebView2 环境 / curl 全局初始化)
    // 返回 false 表示后端不可用
    virtual bool initialize() = 0;

    // 设置代理(必须在 initialize() 前调用)
    virtual void set_proxy(const std::string& proxy_url) = 0;

    // 同步 GET 请求
    // url: 完整 URL
    // headers: 自定义请求头
    virtual HttpResponse get(const std::string& url,
                             const std::map<std::string, std::string>& headers = {}) = 0;

    // 是否已初始化就绪
    virtual bool is_ready() const = 0;

    // 后端名称(用于日志诊断)
    virtual std::string backend_name() const = 0;
};

} // namespace github_research
