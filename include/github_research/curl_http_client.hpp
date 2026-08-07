#pragma once

#include "http_client.hpp"
#include <string>
#include <mutex>

namespace github_research {

// libcurl 后端 HTTP 客户端
// 适用于纯 JSON API 调用(如 GitHub REST API),无浏览器内核依赖
// 优势:无 Edge Runtime 依赖,无进程残留问题,启动快,部署轻量
// 限制:无法执行 JS 渲染,不适用于需要浏览器指纹的反爬虫场景
class CurlHttpClient : public IHttpClient {
public:
    CurlHttpClient(const std::string& user_agent,
                   int timeout_seconds = 30);
    ~CurlHttpClient();

    // 禁止拷贝
    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    // 初始化 curl(主要是设置全局选项,实际 curl 全局初始化在构造函数完成)
    // 返回 false 表示 curl 不可用(头文件/链接库缺失)
    bool initialize() override;

    // 设置代理(必须在首次 get() 前调用)
    // 支持 http/https/socks5 代理
    void set_proxy(const std::string& proxy_url) override { proxy_url_ = proxy_url; }

    // 同步 GET 请求
    // url: 完整 URL
    // headers: 自定义请求头
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers = {}) override;

    // 是否已初始化就绪
    bool is_ready() const override { return ready_; }

    // 后端名称
    std::string backend_name() const override { return "libcurl"; }

private:
    std::string user_agent_;
    int timeout_seconds_;
    std::string proxy_url_;
    bool ready_ = false;
    static std::once_flag global_init_flag_;
    static bool global_init_done_;
};

} // namespace github_research
