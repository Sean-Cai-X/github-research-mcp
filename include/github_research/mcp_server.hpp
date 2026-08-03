#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>
#include "github_client.hpp"

namespace github_research {

using json = nlohmann::json;

// MCP Server over stdio(JSON-RPC 2.0)
// 实现 initialize / tools/list / tools/call 三个方法
class McpServer {
public:
    McpServer(std::optional<std::string> token,
              int timeout_seconds = 30);
    ~McpServer();

    // 设置代理(必须在 run()/run_http() 前调用)
    // proxy_url 格式:http://127.0.0.1:7897
    void set_proxy(const std::string& proxy_url) { client_.set_proxy(proxy_url); }

    // 运行 server,阻塞直到 stdin 关闭或收到 shutdown
    // 返回进程退出码
    int run();

    // 以 HTTP 模式运行,监听指定端口
    // POST /mcp  -> body 为 JSON-RPC 请求
    // GET  /     -> 返回服务状态
    // 阻塞直到 stop() 或出错
    int run_http(int port);

    // 处理单个 HTTP 请求(用于 HTTP 模式)
    // 返回 HTTP 响应体(JSON 字符串)与状态码
    struct HttpResult {
        int status = 200;
        std::string body;
        std::string content_type = "application/json";
    };
    HttpResult handle_http_request(const std::string& method,
                                   const std::string& path,
                                   const std::string& body);

private:
    // 处理单条 JSON-RPC 请求,返回响应 JSON 字符串
    std::string handle_request(const json& request);

    // JSON-RPC 方法分发
    json handle_initialize(const json& params);
    json handle_tools_list();
    json handle_tools_call(const json& params);

    // 读取 stdin 一行(阻塞)
    bool read_line(std::string& line);

    // 写 stdout 一行
    void write_line(const std::string& line);

    // 写 stderr 日志
    void log(const std::string& msg);

    GitHubClient client_;
    bool initialized_ = false;
};

} // namespace github_research
