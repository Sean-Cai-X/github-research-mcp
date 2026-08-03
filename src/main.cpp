#include "github_research/mcp_server.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

static void print_help() {
    std::cerr << "github-research-mcp v0.1.0\n"
              << "GitHub deep research MCP server (WebView2 browser backend)\n\n"
              << "Usage:\n"
              << "  github-research-mcp.exe                     Run as MCP stdio server (default)\n"
              << "  github-research-mcp.exe --port <PORT>       Run as HTTP MCP server on given port\n"
              << "  github-research-mcp.exe --port <PORT> --bind 0.0.0.0  (bind all interfaces)\n"
              << "  github-research-mcp.exe --help              Show this help\n\n"
              << "HTTP endpoints (when --port is used):\n"
              << "  POST /mcp        JSON-RPC 2.0 request (Content-Type: application/json)\n"
              << "  GET  /           Service status\n"
              << "  GET  /tools      List available tools\n\n"
              << "Example:\n"
              << "  github-research-mcp.exe --port 8765\n"
              << "  curl -X POST http://127.0.0.1:8765/mcp \\\n"
              << "       -H 'Content-Type: application/json' \\\n"
              << "       -d '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}'\n\n"
              << "Environment:\n"
              << "  GITHUB_TOKEN             GitHub personal access token (optional)\n"
              << "  GITHUB_RESEARCH_TIMEOUT  Request timeout in seconds (default: 30)\n";
}

int main(int argc, char* argv[]) {
    // 环境变量
    const char* token_env = std::getenv("GITHUB_TOKEN");
    const char* timeout_env = std::getenv("GITHUB_RESEARCH_TIMEOUT");
    int timeout = timeout_env ? std::atoi(timeout_env) : 30;

    std::optional<std::string> token;
    if (token_env && token_env[0] != '\0') {
        token = std::string(token_env);
    }

    // 解析命令行参数
    int port = 0;  // 0 = stdio 模式
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help();
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Invalid port: " << argv[i] << std::endl;
                return 1;
            }
        } else if (arg == "--bind") {
            // 当前实现固定监听 INADDR_ANY,--bind 参数仅用于向前兼容
            ++i;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n\n";
            print_help();
            return 1;
        }
    }

    github_research::McpServer server(token, timeout);

    if (port > 0) {
        return server.run_http(port);
    }
    return server.run();
}
