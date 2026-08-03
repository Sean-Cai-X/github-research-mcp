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
    // 1. WEBVIEW2_USER_DATA_DIR 环境变量(用户显式指定)
    // 2. %LOCALAPPDATA%\github-research-mcp\webview2-data (标准位置,推荐)
    // 3. %TEMP%\github-research-mcp-webview2 (回退,确保可写)
    // 4. exe 同级目录\webview2-data (最后回退)
    std::wstring user_data_dir;
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
            user_data_dir = std::wstring(local_app) + L"\\github-research-mcp\\webview2-data";
        } else {
            // 回退到 %TEMP%
            wchar_t temp_dir[MAX_PATH];
            DWORD t_len = GetEnvironmentVariableW(L"TEMP", temp_dir, MAX_PATH);
            if (t_len > 0 && t_len < MAX_PATH) {
                user_data_dir = std::wstring(temp_dir) + L"\\github-research-mcp-webview2";
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
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,                  // 使用系统 Edge Runtime
        user_data_dir.c_str(),    // 用户数据目录
        nullptr,                  // 默认环境选项
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

// === 同步 GET ===

HttpResponse WebViewClient::get(const std::string& url,
                                const std::map<std::string, std::string>& headers) {
    HttpResponse response;
    if (!ready_.load()) {
        response.body = "webview2 not ready";
        return response;
    }

    completed_ = false;
    fetch_success_ = false;
    fetch_result_.clear();
    fetch_error_.clear();

    execute_fetch(url, headers);

    // pump 消息循环直到完成或超时
    auto start = std::chrono::steady_clock::now();
    while (!completed_.load()) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count() > timeout_seconds_) {
            response.body = "network error: timeout";
            return response;
        }
        Sleep(5);
    }

    if (!fetch_success_) {
        response.body = "javascript execution failed: " + fetch_error_;
        return response;
    }

    // 解析 fetch_result_(JS 返回的 JSON 字符串)
    // 注意:ExecuteScript 返回值是被 JSON 编码过的字符串
    // 即 JS 返回 {status:200,body:"...",headers:{...}} 时,
    // ExecuteScript 回调收到的是 "{\"status\":200,\"body\":\"...\"}"
    // 也就是说,回调收到的 json_result 本身就是合法 JSON,直接 parse 即可
    try {
        json j = json::parse(fetch_result_);
        if (j.contains("status")) {
            response.status_code = j["status"].get<int>();
        }
        if (j.contains("body")) {
            response.body = j["body"].get<std::string>();
        }
        if (j.contains("headers") && j["headers"].is_object()) {
            for (auto it = j["headers"].begin(); it != j["headers"].end(); ++it) {
                response.headers[to_lower(it.key())] = it.value().get<std::string>();
            }
        }
        if (j.contains("error")) {
            response.body = "fetch error: " + j["error"].get<std::string>();
            return response;
        }
    } catch (const std::exception& e) {
        response.body = std::string("invalid response from fetch: ") + e.what();
        return response;
    }

    return response;
}

// === 执行 JS fetch ===

HttpResponse WebViewClient::execute_fetch(const std::string& url,
                                          const std::map<std::string, std::string>& headers) {
    HttpResponse response;

    // 用 nlohmann/json 序列化参数,避免 JS 注入
    json url_json = url;
    json headers_json = json::object();
    for (const auto& [k, v] : headers) {
        headers_json[k] = v;
    }

    // 构建 JS fetch 调用
    // 返回值是 JSON 字符串,包含 status/body/headers 或 error
    std::string js =
        "(async () => {"
        "  try {"
        "    const resp = await fetch(" + url_json.dump() + ", {"
        "      method: 'GET',"
        "      headers: " + headers_json.dump() + ","
        "      credentials: 'omit'"
        "    });"
        "    const text = await resp.text();"
        "    const headers = {};"
        "    resp.headers.forEach((v, k) => { headers[k] = v; });"
        "    return JSON.stringify({ status: resp.status, body: text, headers: headers });"
        "  } catch (e) {"
        "    return JSON.stringify({ error: e.message });"
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
