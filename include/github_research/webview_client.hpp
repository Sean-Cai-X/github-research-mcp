#pragma once

#include <string>
#include <map>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <windows.h>
#include <unknwn.h>
#include "WebView2.h"            // 新版 SDK 已合并 WebView2Environment.h
#include "WebView2EnvironmentOptions.h"

namespace github_research {

// HTTP 响应结构
struct HttpResponse {
    int status_code = 0;
    std::string body;
    std::map<std::string, std::string> headers;  // 小写键
};

// WebView2 浏览器链路 HTTP 客户端
// 利用 Chromium 内核完整浏览器指纹(TLS JA3 / HTTP/2 / Headers 顺序 / JS 执行)
// 通过隐藏窗口 + ExecuteScript(fetch) 实现 headless HTTP 请求
class WebViewClient {
public:
    WebViewClient(const std::string& user_agent,
                  int timeout_seconds = 30,
                  bool use_system_proxy = true);
    ~WebViewClient();

    // 禁止拷贝(COM 对象不可拷贝)
    WebViewClient(const WebViewClient&) = delete;
    WebViewClient& operator=(const WebViewClient&) = delete;

    // 初始化 WebView2 环境(构造后必须调用一次)
    // 返回 false 表示 Edge Runtime 缺失或环境创建失败
    bool initialize();

    // 设置代理(必须在 initialize() 前调用)
    // url 格式:http://127.0.0.1:7897 或 http://user:pass@host:port
    // 传递给 WebView2 的 Chromium 内核 --proxy-server 参数
    void set_proxy(const std::string& proxy_url) { proxy_url_ = proxy_url; }

    // 设置用户数据目录(必须在 initialize() 前调用)
    // 用于隔离 GitHub 后端与其他会话,避免 user data dir 冲突 (0x800700aa)
    // 路径使用 UTF-8,内部转换为宽字符
    void set_user_data_dir(const std::string& dir) { user_data_dir_ = dir; }

    // 关闭并释放资源
    void shutdown();

    // 同步 GET 请求(内部异步 + 消息循环 pump)
    // url: 完整 URL,如 https://api.github.com/repos/owner/repo
    // headers: 自定义请求头
    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers = {});

    // 是否已初始化就绪
    bool is_ready() const { return ready_.load(); }

private:
    // COM 初始化
    bool init_com();
    // 创建隐藏窗口
    bool create_hidden_window();
    // 创建 WebView2 环境(异步)
    bool create_environment();
    // 创建 WebView2 控件(异步)
    bool create_webview();
    // 在页面执行 JS fetch 并等待结果
    HttpResponse execute_fetch(const std::string& url,
                               const std::map<std::string, std::string>& headers);

    // 同步原语(将异步 COM 回调转为同步)
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> ready_{false};
    std::atomic<bool> completed_{false};
    std::atomic<bool> fetch_success_{false};
    std::atomic<bool> nav_completed_{false};  // 首次导航完成
    std::string fetch_result_;        // JS 返回的 JSON 字符串
    std::string fetch_error_;         // JS 异常 message

    // COM 对象(裸指针 + 手动 AddRef/Release,析构时释放)
    // 不用 ComPtr 是为了减少额外依赖,RAII 由析构函数保证
    HWND hwnd_ = nullptr;
    HINSTANCE hinstance_ = nullptr;
    ICoreWebView2Environment* environment_ = nullptr;
    ICoreWebView2Controller* controller_ = nullptr;
    ICoreWebView2* webview_ = nullptr;
    bool com_initialized_ = false;

    std::string user_agent_;
    int timeout_seconds_;
    bool use_system_proxy_;
    std::string proxy_url_;  // 显式代理 URL,空表示不设置
    std::string user_data_dir_;  // 外部指定 user data dir,空表示用默认路径
};

} // namespace github_research
