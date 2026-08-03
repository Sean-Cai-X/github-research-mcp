#include "github_research/mcp_server.hpp"
#include "github_research/string_utils.hpp"
#include "github_research/errors.hpp"
#include "github_research/http_server.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <atomic>

namespace github_research {

// tools.cpp 中实现的 tool 分发函数
json dispatch_tool_call(GitHubClient& client, const json& params);

McpServer::McpServer(std::optional<std::string> token, int timeout_seconds)
    : client_(token, timeout_seconds) {}

McpServer::~McpServer() {}

bool McpServer::read_line(std::string& line) {
    std::getline(std::cin, line);
    return !std::cin.eof();
}

void McpServer::write_line(const std::string& line) {
    std::cout << line << "\n";
    std::cout.flush();
}

void McpServer::log(const std::string& msg) {
    std::cerr << "[mcp] " << msg << std::endl;
}

int McpServer::run() {
    log("server starting");

    // 初始化 WebView2(一次)
    if (!client_.is_ready()) {
        // WebView2 在首次 HTTP 请求时延迟初始化,这里不阻塞
        log("webview2 will be initialized on first request");
    }

    std::string line;
    while (read_line(line)) {
        if (line.empty()) continue;

        json request;
        try {
            request = json::parse(line);
        } catch (const std::exception& e) {
            json err = {
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {{"code", -32700}, {"message", std::string("Parse error: ") + e.what()}}}
            };
            write_line(err.dump());
            continue;
        }

        std::string response = handle_request(request);
        if (!response.empty()) {
            write_line(response);
        }
    }

    log("server shutting down");
    return 0;
}

std::string McpServer::handle_request(const json& request) {
    // 校验 JSON-RPC 2.0
    if (!request.is_object() || !request.contains("jsonrpc")) {
        json err = {
            {"jsonrpc", "2.0"},
            {"id", request.contains("id") ? request["id"] : json(nullptr)},
            {"error", {{"code", -32600}, {"message", "Invalid Request"}}}
        };
        return err.dump();
    }

    json id = request.contains("id") ? request["id"] : json(nullptr);
    std::string method = request.value("method", std::string());

    json result;
    try {
        if (method == "initialize") {
            result = handle_initialize(request.value("params", json::object()));
            initialized_ = true;
        } else if (method == "initialized" || method == "notifications/initialized") {
            // 通知,无响应
            return "";
        } else if (method == "tools/list") {
            result = handle_tools_list();
        } else if (method == "tools/call") {
            result = handle_tools_call(request.value("params", json::object()));
        } else if (method == "shutdown") {
            json err = {{"jsonrpc", "2.0"}, {"id", id}, {"result", nullptr}};
            return err.dump();
        } else if (method == "ping") {
            result = {{"pong", true}};
        } else {
            json err = {
                {"jsonrpc", "2.0"},
                {"id", id},
                {"error", {{"code", -32601}, {"message", "Method not found: " + method}}}
            };
            return err.dump();
        }
    } catch (const std::exception& e) {
        json err = {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error", {{"code", -32603}, {"message", std::string("Internal error: ") + e.what()}}}
        };
        return err.dump();
    }

    json response = {
        {"jsonrpc", "2.0"},
        {"id", id},
        {"result", result}
    };
    return response.dump();
}

json McpServer::handle_initialize(const json& params) {
    return {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", json::object({{"tools", json::object()}})},
        {"serverInfo", {
            {"name", "github-research-mcp"},
            {"version", "0.1.0"}
        }}
    };
}

json McpServer::handle_tools_list() {
    // 10 个 tool 的 schema,严格对齐规范
    return {
        {"tools", json::array({
            {
                {"name", "github_get_repo_info"},
                {"description", "Get basic repository information (name, description, stars, forks, etc.)."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner (user or org)"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_readme"},
                {"description", "Get repository README content as markdown text."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_tree"},
                {"description", "Get repository directory tree as formatted text."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"branch", {{"type", "string"}, {"default", "main"}, {"description", "Branch name (auto-fallback to master if main fails)"}}},
                        {"max_depth", {{"type", "integer"}, {"default", 3}, {"minimum", 1}, {"maximum", 10}}},
                        {"recursive", {{"type", "boolean"}, {"default", true}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_languages"},
                {"description", "Get repository languages and their byte counts."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_contributors"},
                {"description", "Get repository contributors list."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"limit", {{"type", "integer"}, {"default", 30}, {"minimum", 1}, {"maximum", 100}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_commits"},
                {"description", "Get recent commits of a GitHub repository. Use for timeline reconstruction and activity analysis. Use branch/sha to query non-default branches."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"limit", {{"type", "integer"}, {"default", 50}, {"minimum", 1}, {"maximum", 100}}},
                        {"since", {{"type", "string"}, {"format", "date-time"}, {"description", "ISO 8601 datetime, only commits after this"}}},
                        {"branch", {{"type", "string"}, {"description", "Branch name to query (e.g. codex/cxcore-integration)"}}},
                        {"sha", {{"type", "string"}, {"description", "Override: branch name, tag name, or commit SHA (takes priority over branch)"}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_branches"},
                {"description", "List all branches of a repository. Use this before per-branch commit aggregation to discover branch names including those with slashes."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"limit", {{"type", "integer"}, {"default", 100}, {"minimum", 1}, {"maximum", 100}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_issues"},
                {"description", "Get repository issues (excludes PRs unless state filter includes them)."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"state", {{"type", "string"}, {"default", "all"}, {"enum", json::array({"open", "closed", "all"})}}},
                        {"limit", {{"type", "integer"}, {"default", 30}, {"minimum", 1}, {"maximum", 100}}},
                        {"labels", {{"type", "string"}, {"description", "Comma-separated label names"}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_pull_requests"},
                {"description", "Get repository pull requests list."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"state", {{"type", "string"}, {"default", "all"}, {"enum", json::array({"open", "closed", "all"})}}},
                        {"limit", {{"type", "integer"}, {"default", 30}, {"minimum", 1}, {"maximum", 100}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_get_releases"},
                {"description", "Get repository releases list."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}},
                        {"limit", {{"type", "integer"}, {"default", 10}, {"minimum", 1}, {"maximum", 100}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_summarize_repo"},
                {"description", "Get comprehensive repository summary (info, languages, contributor_count, latest_release)."},
                {"inputSchema", {
                    {"type", "object"},
                    {"properties", {
                        {"owner", {{"type", "string"}, {"description", "Repository owner"}}},
                        {"repo", {{"type", "string"}, {"description", "Repository name"}}}
                    }},
                    {"required", json::array({"owner", "repo"})}
                }}
            },
            {
                {"name", "github_search_repositories"},
                {"description", "Search repositories by project name, topic, language, stars, etc. Use for discovery (e.g. find popular C++ vision libraries)."},
                {"inputSchema", json::object({
                    {"type", "object"},
                    {"properties", json::object({
                        {"q", json::object({
                            {"type", "string"},
                            {"description", "GitHub search query. Supports qualifiers: language:C++, stars:>1000, topic:cv, user:octocat, pushed:>2025-01-01, fork:true, license:MIT, size:>1000"}
                        })},
                        {"sort", json::object({
                            {"type", "string"},
                            {"enum", json::array({"stars", "forks", "updated"})},
                            {"description", "Sort field (omit for best-match)"}
                        })},
                        {"order", json::object({
                            {"type", "string"},
                            {"default", "desc"},
                            {"enum", json::array({"asc", "desc"})}
                        })},
                        {"limit", json::object({
                            {"type", "integer"},
                            {"default", 30},
                            {"minimum", 1},
                            {"maximum", 100}
                        })},
                        {"page", json::object({
                            {"type", "integer"},
                            {"default", 1},
                            {"minimum", 1},
                            {"maximum", 10}
                        })}
                    })},
                    {"required", json::array({"q"})}
                })}
            },
            {
                {"name", "github_search_users"},
                {"description", "Search GitHub users/orgs by name, location, language, followers, etc. Use to discover authors or organizations matching a research topic."},
                {"inputSchema", json::object({
                    {"type", "object"},
                    {"properties", json::object({
                        {"q", json::object({
                            {"type", "string"},
                            {"description", "GitHub user search query. Supports: type:user, type:org, followers:>100, location:China, language:C++, repos:>50, created:<2015-01-01"}
                        })},
                        {"sort", json::object({
                            {"type", "string"},
                            {"enum", json::array({"followers", "repositories", "joined"})},
                            {"description", "Sort field (omit for best-match)"}
                        })},
                        {"order", json::object({
                            {"type", "string"},
                            {"default", "desc"},
                            {"enum", json::array({"asc", "desc"})}
                        })},
                        {"limit", json::object({
                            {"type", "integer"},
                            {"default", 30},
                            {"minimum", 1},
                            {"maximum", 100}
                        })},
                        {"page", json::object({
                            {"type", "integer"},
                            {"default", 1},
                            {"minimum", 1},
                            {"maximum", 10}
                        })}
                    })},
                    {"required", json::array({"q"})}
                })}
            }
        })}
    };
}

// tools/call 的具体分发在 tools.cpp 中实现
json McpServer::handle_tools_call(const json& params) {
    return dispatch_tool_call(client_, params);
}

// === HTTP 模式 ===

McpServer::HttpResult McpServer::handle_http_request(const std::string& method,
                                                     const std::string& path,
                                                     const std::string& body) {
    HttpResult result;

    // GET / -> 服务状态
    if (method == "GET" && (path == "/" || path == "/health")) {
        result.body = json{
            {"service", "github-research-mcp"},
            {"version", "0.1.0"},
            {"mode", "http"},
            {"status", "ok"},
            {"endpoints", {
                {"POST /mcp", "JSON-RPC 2.0 request"},
                {"GET /", "service status"},
                {"GET /tools", "list available tools"}
            }}
        }.dump();
        result.content_type = "application/json";
        return result;
    }

    // GET /tools -> 工具列表(便于调试)
    if (method == "GET" && path == "/tools") {
        json list = handle_tools_list();
        result.body = list.dump();
        result.content_type = "application/json";
        return result;
    }

    // POST /mcp -> JSON-RPC 请求
    if (method == "POST" && (path == "/mcp" || path == "/")) {
        json request;
        try {
            request = json::parse(body);
        } catch (const std::exception& e) {
            result.status = 400;
            result.body = json{
                {"jsonrpc", "2.0"},
                {"id", nullptr},
                {"error", {{"code", -32700}, {"message", std::string("Parse error: ") + e.what()}}}
            }.dump();
            return result;
        }

        // 支持批量请求(JSON 数组)
        if (request.is_array()) {
            json responses = json::array();
            for (auto& single : request) {
                std::string resp = handle_request(single);
                if (!resp.empty()) {
                    try {
                        responses.push_back(json::parse(resp));
                    } catch (...) {
                        // skip unparseable
                    }
                }
            }
            result.body = responses.dump();
            return result;
        }

        std::string resp = handle_request(request);
        if (resp.empty()) {
            // 通知类请求,无响应
            result.status = 202;
            result.body = "";
            return result;
        }
        result.body = resp;
        return result;
    }

    // 其它路径
    result.status = 404;
    result.body = json{{"error", "not found"}, {"path", path}}.dump();
    return result;
}

int McpServer::run_http(int port) {
    log("server starting in HTTP mode on port " + std::to_string(port));

    if (!client_.is_ready()) {
        log("webview2 will be initialized on first request");
    }

    // 用指针以便 lambda 能引用 server 并触发 stop()
    HttpServer* server_ptr = nullptr;
    HttpServer server(port, [this, &server_ptr](const HttpRequest& req) -> HttpServerResponse {
        HttpServerResponse http_resp;
        McpServer::HttpResult mcp_resp = handle_http_request(req.method, req.path, req.body);
        http_resp.status = mcp_resp.status;
        http_resp.body = mcp_resp.body;
        http_resp.content_type = mcp_resp.content_type;

        // 检测 shutdown 请求 -> 触发 server 停止
        if (req.method == "POST" && req.path == "/mcp") {
            try {
                json j = json::parse(req.body);
                if (j.is_object() && j.value("method", std::string()) == "shutdown") {
                    if (server_ptr) server_ptr->stop();
                }
            } catch (...) {
                // ignore
            }
        }
        return http_resp;
    });
    server_ptr = &server;

    int rc = server.run();
    log("http server shutting down");
    return rc;
}

} // namespace github_research
