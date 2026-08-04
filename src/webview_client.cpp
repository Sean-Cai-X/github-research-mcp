#include "github_research/webview_client.hpp"
#include "github_research/string_utils.hpp"
#include <windows.h>
#include <objbase.h>
#include <winreg.h>
#include <wrl.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>

namespace github_research {

using json = nlohmann::json;
using Microsoft::WRL::Callback;

// === 隐藏窗口过程 ===
static LRESULT CALLBACK HiddenWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// === 构造/析构 ===

WebViewClient::WebViewClient(const std::string& user_agent,
                             int timeout_seconds,
                             bool use_system_proxy)
    : user_agent_(user_agent),
      timeout_seconds_(timeout_seconds),
      use_system_proxy_(use_system_proxy) {}

WebViewClient::~WebViewClient() {
    shutdown();
}

bool WebViewClient::init_com() {
    if (com_initialized_) return true;
    // STA 模式(WebView2 要求)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // RPC_E_CHANGED_MODE 表示已经是 COM 初始化过,忽略
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        return false;
    }
    com_initialized_ = true;
    return true;
}

bool WebViewClient::create_hidden_window() {
    hinstance_ = GetModuleHandleW(nullptr);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = HiddenWndProc;
    wc.hInstance = hinstance_;
    wc.lpszClassName = L"GitHubResearchMcpHiddenWnd";
    // 注册可能已存在,忽略返回值
    RegisterClassW(&wc);

    // WebView2 controller 需要非零大小窗口,用 1x1 + WS_POPUP 避免触发 0x8000ffff
    hwnd_ = CreateWindowExW(
        0, wc.lpszClassName, L"Hidden",
        WS_POPUP,  // popup 样式,无标题栏
        0, 0, 1, 1,  // 1x1 像素
        nullptr, nullptr, hinstance_, nullptr
    );
    return hwnd_ != nullptr;
}

bool WebViewClient::create_environment() {
    // 用户数据目录选择策略(按优先级):
    // 0. 外部 set_user_data_dir() 指定的目录(优先,用于多实例隔离)
    // 1. WEBVIEW2_USER_DATA_DIR 环境变量(用户显式指定)
    // 2. %LOCALAPPDATA%\research-mcp\webview2-data (标准位置,推荐)
    // 3. %TEMP%\research-mcp-webview2 (回退,确保可写)
    // 4. exe 同级目录\webview2-data (最后回退)
    std::wstring user_data_dir;
    if (!user_data_dir_.empty()) {
        // 优先使用外部指定的目录(用于 8 源会话隔离)
        user_data_dir = to_wstring(user_data_dir_);
        std::cerr << "[webview] using external user data dir: ";
        std::wcerr << user_data_dir << std::endl;
    } else {
        wchar_t env_dir[MAX_PATH];
        DWORD env_len = GetEnvironmentVariableW(L"WEBVIEW2_USER_DATA_DIR", env_dir, MAX_PATH);
        if (env_len > 0 && env_len < MAX_PATH) {
            user_data_dir = env_dir;
            std::cerr << "[webview] using WEBVIEW2_USER_DATA_DIR: ";
            std::wcerr << user_data_dir << std::endl;
        } else {
            // 尝试 %LOCALAPPDATA%
            wchar_t local_app[MAX_PATH];
            DWORD la_len = GetEnvironmentVariableW(L"LOCALAPPDATA", local_app, MAX_PATH);
            if (la_len > 0 && la_len < MAX_PATH) {
                user_data_dir = std::wstring(local_app) + L"\\research-mcp\\webview2-data";
            } else {
                // 回退到 %TEMP%
                wchar_t temp_dir[MAX_PATH];
                DWORD t_len = GetEnvironmentVariableW(L"TEMP", temp_dir, MAX_PATH);
                if (t_len > 0 && t_len < MAX_PATH) {
                    user_data_dir = std::wstring(temp_dir) + L"\\research-mcp-webview2";
                } else {
                    // 最后回退:exe 同级目录
                    wchar_t exe_path[MAX_PATH];
                    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
                    std::wstring exe_dir = exe_path;
                    size_t last_sep = exe_dir.find_last_of(L"\\/");
                    if (last_sep != std::wstring::npos) {
                        exe_dir = exe_dir.substr(0, last_sep);
                    }
                    user_data_dir = exe_dir + L"\\webview2-data";
                }
            }
        }
    }

    // 创建目录(如果不存在)
    // 递归创建:先创建父目录,再创建子目录
    {
        std::wstring parent = user_data_dir;
        size_t last_sep = parent.find_last_of(L"\\/");
        if (last_sep != std::wstring::npos) {
            parent = parent.substr(0, last_sep);
            CreateDirectoryW(parent.c_str(), nullptr);
        }
    }
    BOOL dir_created = CreateDirectoryW(user_data_dir.c_str(), nullptr);
    if (!dir_created) {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            std::cerr << "[webview] WARNING: failed to create user data dir (err=" << err << "): ";
            std::wcerr << user_data_dir << std::endl;
        }
    }
    std::cerr << "[webview] user data dir: ";
    std::wcerr << user_data_dir << std::endl;

    // 检查 Edge Runtime 是否安装(诊断信息)
    HKEY runtime_key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
            0, KEY_READ, &runtime_key) == ERROR_SUCCESS) {
        wchar_t version[64] = {0};
        DWORD version_size = sizeof(version);
        if (RegQueryValueExW(runtime_key, L"pv", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(version), &version_size) == ERROR_SUCCESS) {
            std::cerr << "[webview] Edge Runtime version: ";
            std::wcerr << version << std::endl;
        }
        RegCloseKey(runtime_key);
    } else {
        std::cerr << "[webview] WARNING: WebView2 Runtime NOT FOUND in registry" << std::endl;
        std::cerr << "[webview] Please install from: https://developer.microsoft.com/microsoft-edge/webview2/" << std::endl;
    }

    ready_ = false;

    // 创建环境选项(用于传递 --proxy-server 等 Chromium 命令行参数)
    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> options_ptr;
    if (!proxy_url_.empty()) {
        // Chromium 代理参数:--proxy-server=<url>
        // 注意:Chromium 不接受 http:// 前缀的 scheme,需要去掉
        std::string proxy = proxy_url_;
        const std::string http_prefix = "http://";
        if (proxy.compare(0, http_prefix.size(), http_prefix) == 0) {
            proxy = proxy.substr(http_prefix.size());
        }
        std::wstring additional_args = L"--proxy-server=" + to_wstring(proxy);
        std::cerr << "[webview] using proxy: " << proxy_url_ << std::endl;

        // 使用 Microsoft::WRL::Make 创建 CoreWebView2EnvironmentOptions 实例
        auto opts = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        if (opts) {
            opts->put_AdditionalBrowserArguments(additional_args.c_str());
            options_ptr = opts;
        } else {
            std::cerr << "[webview] WARNING: failed to create environment options, proxy not applied" << std::endl;
        }
    }

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                  // 使用系统 Edge Runtime
        user_data_dir.c_str(),    // 用户数据目录
        options_ptr.Get(),        // 环境选项(含代理设置,可能为 nullptr)
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    std::cerr << "[webview] create environment failed: 0x"
                              << std::hex << result << std::endl;
                    ready_ = false;
                    cv_.notify_one();
                    return result;
                }
                environment_ = env;
                environment_->AddRef();
                return create_webview();
            }
        ).Get()
    );
    // options_ptr 在超出作用域时自动释放(ComPtr RAII)
    if (FAILED(hr)) {
        std::cerr << "[webview] CreateCoreWebView2EnvironmentWithOptions failed: 0x"
                  << std::hex << hr << std::endl;
        return false;
    }
    return true;
}

bool WebViewClient::create_webview() {
    HRESULT hr = environment_->CreateCoreWebView2Controller(
        hwnd_,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                if (FAILED(result) || !controller) {
                    std::cerr << "[webview] create controller failed: 0x"
                              << std::hex << result << std::endl;
                    ready_ = false;
                    cv_.notify_one();
                    return result;
                }
                controller_ = controller;
                controller_->AddRef();
                // 必须设置 Bounds,否则 WebView2 可能无法正常工作
                RECT bounds = {0, 0, 1, 1};
                controller_->put_Bounds(bounds);
                controller_->get_CoreWebView2(&webview_);
                if (webview_) {
                    webview_->AddRef();
                    // 注册导航完成事件,导航到 api.github.com 建立合法 origin
                    // 避免 about:blank origin=null 导致 fetch 被浏览器阻止
                    webview_->add_NavigationCompleted(
                        Callback<ICoreWebView2NavigationCompletedEventHandler>(
                            [this](ICoreWebView2* /*sender*/, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                BOOL success = FALSE;
                                if (args) args->get_IsSuccess(&success);
                                if (success) {
                                    nav_completed_.store(true);
                                } else {
                                    std::cerr << "[webview] initial navigation failed" << std::endl;
                                    // 即使导航失败也标记完成,让 fetch 尝试(可能 CORS 仍可用)
                                    nav_completed_.store(true);
                                }
                                cv_.notify_one();
                                return S_OK;
                            }
                        ).Get(),
                        nullptr);
                    // 标记就绪(在 Navigate 之前,避免 ready_ 永远为 false 导致 initialize 超时)
                    ready_.store(true);
                    // 导航到 GitHub API 建立合法 https origin
                    webview_->Navigate(L"https://api.github.com");
                }
                cv_.notify_one();
                return S_OK;
            }
        ).Get()
    );
    if (FAILED(hr)) {
        std::cerr << "[webview] CreateCoreWebView2Controller failed: 0x"
                  << std::hex << hr << std::endl;
        cv_.notify_one();
    }
    return SUCCEEDED(hr);
}

bool WebViewClient::initialize() {
    if (ready_.load()) return true;

    if (!init_com()) {
        std::cerr << "[webview] COM init failed" << std::endl;
        return false;
    }
    if (!create_hidden_window()) {
        std::cerr << "[webview] create hidden window failed" << std::endl;
        return false;
    }
    if (!create_environment()) {
        std::cerr << "[webview] create environment failed (Edge Runtime missing?)" << std::endl;
        return false;
    }

    // pump 消息循环直到环境与控件就绪,或超时
    auto start = std::chrono::steady_clock::now();
    const int init_timeout = 30;  // 初始化最长等 30s
    while (!ready_.load()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() > init_timeout) {
            std::cerr << "[webview] initialize timeout (controller not ready)" << std::endl;
            return false;
        }
        Sleep(10);
    }

    // 等待首次导航到 https://api.github.com 完成,建立合法 origin
    // 避免 about:blank 的 null origin 导致 fetch 被 CORS 阻止
    auto nav_start = std::chrono::steady_clock::now();
    const int nav_timeout = 30;  // 导航最长等 30s
    while (!nav_completed_.load()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - nav_start).count() > nav_timeout) {
            std::cerr << "[webview] initial navigation timeout" << std::endl;
            // 导航超时不一定是致命错误,继续尝试 fetch
            break;
        }
        Sleep(10);
    }

    std::cerr << "[webview] initialized (origin: https://api.github.com)" << std::endl;
    return ready_.load();
}

void WebViewClient::shutdown() {
    if (webview_) {
        webview_->Release();
        webview_ = nullptr;
    }
    if (controller_) {
        controller_->Close();
        controller_->Release();
        controller_ = nullptr;
    }
    if (environment_) {
        environment_->Release();
        environment_ = nullptr;
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
}

// === 同步 GET(Navigate 模式,与 arXiv 单一技术栈一致) ===

HttpResponse WebViewClient::get(const std::string& url,
                                const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    if (!ready_.load()) {
        response.body = "webview2 not ready";
        return response;
    }

    // 单一技术栈:Navigate + ExecuteScript(与 arXiv/HN 等源完全一致)
    // 1. Navigate 到 API URL(api.github.com 返回 JSON,浏览器渲染为 <pre>)
    // 2. 等待 NavigationCompleted
    // 3. ExecuteScript 读取 document.body.innerText(获取 JSON 文本)
    // 不使用 fetch/XHR(ExecuteScript 不 await Promise,同步 XHR 被 Chromium 限制)

    // 重置导航状态
    nav_completed_.store(false);

    // 1. Navigate
    std::wstring wurl = to_wstring(url);
    HRESULT hr = webview_->Navigate(wurl.c_str());
    if (FAILED(hr)) {
        response.body = "Navigate failed: 0x" + std::to_string(static_cast<unsigned long>(hr));
        return response;
    }

    // 2. 等待 NavigationCompleted(pump 消息循环)
    auto start = std::chrono::steady_clock::now();
    while (!nav_completed_.load()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_seconds_) {
            response.body = "network error: navigation timeout";
            return response;
        }
        Sleep(5);
    }

    // 3. ExecuteScript 读取页面内容
    // api.github.com 返回 JSON,浏览器以 <pre> 标签渲染
    // document.body.innerText 能拿到 JSON 文本
    completed_ = false;
    fetch_success_ = false;
    fetch_result_.clear();

    std::string js = "(function(){ return document.body.innerText; })()";
    std::wstring wjs = to_wstring(js);

    hr = webview_->ExecuteScript(
        wjs.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this](HRESULT exec_hr, LPCWSTR json_result) -> HRESULT {
                if (SUCCEEDED(exec_hr) && json_result) {
                    fetch_result_ = to_utf8(json_result);
                    fetch_success_ = true;
                } else {
                    fetch_error_ = "ExecuteScript HRESULT 0x" +
                                   std::to_string(static_cast<unsigned long>(exec_hr));
                }
                completed_ = true;
                cv_.notify_one();
                return S_OK;
            }
        ).Get()
    );
    if (FAILED(hr)) {
        response.body = "ExecuteScript dispatch failed: 0x" +
                        std::to_string(static_cast<unsigned long>(hr));
        return response;
    }

    // pump 消息循环等待 ExecuteScript 完成
    start = std::chrono::steady_clock::now();
    while (!completed_.load()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_seconds_) {
            response.body = "network error: ExecuteScript timeout";
            return response;
        }
        Sleep(5);
    }

    if (!fetch_success_) {
        response.body = "javascript execution failed: " + fetch_error_;
        return response;
    }

    // 解析 ExecuteScript 返回值
    // ExecuteScript 对 JS 字符串返回值会再 JSON 编码一次(加引号 + 转义)
    // 所以 fetch_result_ 是 "\"{ ... }\"" 形式,需要先 parse 外层字符串
    try {
        json j = json::parse(fetch_result_);
        if (j.is_string()) {
            // 外层是 JSON 字符串,取出内层文本
            response.body = j.get<std::string>();
        } else {
            // 直接是对象(不太可能,但处理一下)
            response.body = j.dump();
        }
        // api.github.com 成功返回 200,body 是合法 JSON
        // 如果 body 包含 "message" 且有 "documentation_url",通常是错误响应
        try {
            json body_json = json::parse(response.body);
            if (body_json.contains("message") && body_json.contains("documentation_url")) {
                response.status_code = 403;  // rate limit or error
            } else {
                response.status_code = 200;
            }
        } catch (...) {
            response.status_code = 200;  // 非 JSON body,假设成功
        }
    } catch (const std::exception& e) {
        response.body = std::string("invalid response: ") + e.what() +
                        " raw=" + fetch_result_.substr(0, 200);
        return response;
    }

    return response;
}

// === 执行 JS fetch(已弃用,保留接口兼容) ===

HttpResponse WebViewClient::execute_fetch(const std::string& url,
                                          const std::map<std::string, std::string>& headers) {
    HttpResponse response;

    // 用 nlohmann/json 序列化参数,避免 JS 注入
    json url_json = url;
    json headers_json = json::object();
    for (const auto& [k, v] : headers) {
        headers_json[k] = v;
    }

    // 构建 JS 调用:同步 XMLHttpRequest(WebView2 ExecuteScript 对 async/Promise 返回值不可靠)
    // 同步 XHR 会阻塞 JS 执行直到响应到达,然后 return 字符串给 ExecuteScript 回调
    // 注意:Chromium 仍支持同步 XHR(仅 main thread),返回字符串会被 WebView2 再 JSON 编码一次
    std::string js =
        "(function(){"
        "  try {"
        "    var xhr = new XMLHttpRequest();"
        "    xhr.open('GET', " + url_json.dump() + ", false);"  // false = 同步
        "    var hdrs = " + headers_json.dump() + ";"
        "    for (var k in hdrs) { if (hdrs.hasOwnProperty(k)) xhr.setRequestHeader(k, hdrs[k]); }"
        "    xhr.send(null);"
        "    var respHeaders = {};"
        "    var allHeaders = xhr.getAllResponseHeaders() || '';"
        "    var lines = allHeaders.split('\\r\\n');"
        "    for (var i = 0; i < lines.length; i++) {"
        "      var idx = lines[i].indexOf(':');"
        "      if (idx > 0) {"
        "        var key = lines[i].substring(0, idx).replace(/^\\s+|\\s+$/g, '').toLowerCase();"
        "        var val = lines[i].substring(idx + 1).replace(/^\\s+|\\s+$/g, '');"
        "        respHeaders[key] = val;"
        "      }"
        "    }"
        "    var result = { status: xhr.status, body: xhr.responseText, headers: respHeaders, dbg: 'xhr_ok' };"
        "    return JSON.stringify(result);"
        "  } catch (e) {"
        "    return JSON.stringify({ error: String(e && e.message ? e.message : e), dbg: 'xhr_catch' });"
        "  }"
        "})()";

    std::wstring wjs = to_wstring(js);

    HRESULT hr = webview_->ExecuteScript(
        wjs.c_str(),
        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this](HRESULT exec_hr, LPCWSTR json_result) -> HRESULT {
                if (FAILED(exec_hr) || !json_result) {
                    fetch_success_ = false;
                    fetch_error_ = "ExecuteScript HRESULT 0x" +
                                   std::to_string(static_cast<unsigned long>(exec_hr));
                    completed_ = true;
                    cv_.notify_one();
                    return exec_hr;
                }
                fetch_result_ = to_utf8(json_result);
                fetch_success_ = true;
                completed_ = true;
                cv_.notify_one();
                return S_OK;
            }
        ).Get()
    );
    if (FAILED(hr)) {
        fetch_success_ = false;
        fetch_error_ = "ExecuteScript dispatch failed 0x" +
                       std::to_string(static_cast<unsigned long>(hr));
        completed_ = true;
    }

    return response;  // 真正结果通过 fetch_result_ 在 get() 中解析
}

} // namespace github_research
