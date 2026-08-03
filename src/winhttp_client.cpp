#include "github_research/winhttp_client.hpp"
#include "github_research/string_utils.hpp"

#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#pragma comment(lib, "winhttp.lib")

#include <iostream>
#include <vector>

namespace github_research {

WinHttpClient::WinHttpClient(const std::string& user_agent,
                             int timeout_seconds,
                             bool use_system_proxy)
    : user_agent_(user_agent),
      timeout_seconds_(timeout_seconds),
      use_system_proxy_(use_system_proxy) {}

WinHttpClient::~WinHttpClient() {}

// 解析 URL,分离 scheme/host/port/path
static bool parse_url(const std::string& url,
                      bool& is_https,
                      std::wstring& host,
                      int& port,
                      std::wstring& path) {
    is_https = false;
    port = 80;
    path = L"/";

    std::string url_str = url;
    size_t scheme_end = std::string::npos;
    if (url_str.compare(0, 8, "https://") == 0) {
        is_https = true;
        port = 443;
        scheme_end = 8;
    } else if (url_str.compare(0, 7, "http://") == 0) {
        is_https = false;
        port = 80;
        scheme_end = 7;
    } else {
        return false;
    }

    std::string rest = url_str.substr(scheme_end);
    size_t path_pos = rest.find('/');
    std::string host_port = (path_pos == std::string::npos) ? rest : rest.substr(0, path_pos);
    if (path_pos != std::string::npos) {
        path = to_wstring(rest.substr(path_pos));
    }

    // 解析 host:port
    size_t colon = host_port.find(':');
    if (colon != std::string::npos) {
        host = to_wstring(host_port.substr(0, colon));
        try {
            port = std::stoi(host_port.substr(colon + 1));
        } catch (...) {
            return false;
        }
    } else {
        host = to_wstring(host_port);
    }

    return !host.empty();
}

WinHttpResponse WinHttpClient::get(const std::string& url,
                                   const std::map<std::string, std::string>& headers) {
    WinHttpResponse response;

    bool is_https = false;
    std::wstring host, path;
    int port = 80;
    if (!parse_url(url, is_https, host, port, path)) {
        response.body = "invalid url: " + url;
        return response;
    }

    // 初始化 WinHTTP
    std::wstring wuser_agent = to_wstring(user_agent_);
    HINTERNET h_session = WinHttpOpen(
        wuser_agent.c_str(),
        use_system_proxy_ ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );
    if (!h_session) {
        response.body = "WinHttpOpen failed: " + std::to_string(GetLastError());
        return response;
    }

    // 设置超时
    WinHttpSetTimeouts(h_session,
                       timeout_seconds_ * 1000,  // resolve
                       timeout_seconds_ * 1000,  // connect
                       timeout_seconds_ * 1000,  // send
                       timeout_seconds_ * 1000); // receive

    // 连接
    HINTERNET h_connect = WinHttpConnect(h_session, host.c_str(),
                                         static_cast<INTERNET_PORT>(port), 0);
    if (!h_connect) {
        response.body = "WinHttpConnect failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(h_session);
        return response;
    }

    // 创建请求
    DWORD flags = WINHTTP_FLAG_REFRESH;
    if (is_https) flags |= WINHTTP_FLAG_SECURE;

    HINTERNET h_request = WinHttpOpenRequest(
        h_connect,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );
    if (!h_request) {
        response.body = "WinHttpOpenRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return response;
    }

    // 添加请求头
    std::wstring header_str;
    for (const auto& [k, v] : headers) {
        header_str += to_wstring(k) + L": " + to_wstring(v) + L"\r\n";
    }
    if (!header_str.empty()) {
        WinHttpAddRequestHeaders(h_request, header_str.c_str(),
                                 static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
    }

    // 发送请求
    BOOL b_results = WinHttpSendRequest(h_request,
                                        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!b_results) {
        response.body = "WinHttpSendRequest failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return response;
    }

    // 接收响应
    b_results = WinHttpReceiveResponse(h_request, nullptr);
    if (!b_results) {
        response.body = "WinHttpReceiveResponse failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(h_request);
        WinHttpCloseHandle(h_connect);
        WinHttpCloseHandle(h_session);
        return response;
    }

    // 获取状态码
    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (WinHttpQueryHeaders(h_request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        response.status_code = static_cast<int>(status_code);
    }

    // 获取响应头(可选,简化处理)
    DWORD header_size = 0;
    WinHttpQueryHeaders(h_request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        nullptr, &header_size, WINHTTP_NO_HEADER_INDEX);
    if (header_size > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<wchar_t> header_buf(header_size / sizeof(wchar_t) + 1, 0);
        if (WinHttpQueryHeaders(h_request, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                header_buf.data(), &header_size, WINHTTP_NO_HEADER_INDEX)) {
            std::string raw_headers = to_utf8(header_buf.data());
            // 简单解析每个 header 行
            size_t pos = 0;
            while (pos < raw_headers.size()) {
                size_t eol = raw_headers.find("\r\n", pos);
                if (eol == std::string::npos) break;
                std::string line = raw_headers.substr(pos, eol - pos);
                pos = eol + 2;
                size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    std::string k = to_lower(line.substr(0, colon));
                    std::string v = line.substr(colon + 1);
                    // 去掉首尾空白
                    size_t s = v.find_first_not_of(" \t");
                    size_t e = v.find_last_not_of(" \t\r\n");
                    if (s != std::string::npos) {
                        response.headers[k] = v.substr(s, e - s + 1);
                    }
                }
            }
        }
    }

    // 读取响应体
    std::string body;
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;
    do {
        bytes_available = 0;
        if (!WinHttpQueryDataAvailable(h_request, &bytes_available)) {
            break;
        }
        if (bytes_available == 0) break;

        std::vector<char> buf(bytes_available + 1, 0);
        if (!WinHttpReadData(h_request, buf.data(), bytes_available, &bytes_read)) {
            break;
        }
        if (bytes_read == 0) break;
        body.append(buf.data(), bytes_read);
    } while (bytes_read > 0);

    response.body = body;

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);

    return response;
}

} // namespace github_research
