#pragma once

#include <string>
#include <map>

namespace github_research {

// WinHTTP 后端的 HTTP 响应(与 webview_client.hpp 中的 HttpResponse 结构兼容)
struct WinHttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;
};

// WinHTTP 后端 HTTP 客户端
// 当 WebView2 初始化失败时作为 fallback 使用
// 优点:无依赖、快速、稳定
// 缺点:无浏览器指纹(TLS JA3/HTTP2),反爬能力弱
class WinHttpClient {
public:
    WinHttpClient(const std::string& user_agent,
                  int timeout_seconds,
                  bool use_system_proxy);
    ~WinHttpClient();

    // GET 请求
    WinHttpResponse get(const std::string& url,
                        const std::map<std::string, std::string>& headers);

private:
    std::string user_agent_;
    int timeout_seconds_;
    bool use_system_proxy_;
};

} // namespace github_research
