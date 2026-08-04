#include "github_research/mcp_server.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

static void print_help() {
    std::cerr << "research-mcp v0.2.0\n"
              << "8-source research MCP server (unified WebView2 backend)\n"
              << "Sources: GitHub + arXiv + HackerNews + npm/PyPI + PapersWithCode\n"
              << "         + HuggingFace + SemanticScholar + StackOverflow\n\n"
              << "Usage:\n"
              << "  research-mcp.exe [options]\n\n"
              << "Options:\n"
              << "  --port <PORT>              HTTP MCP server port (default: stdio mode)\n"
              << "  --proxy <URL>              Proxy URL (applies to all sessions)\n"
              << "  --gh-profile <DIR>         GitHub WebView user data dir (8-source isolation)\n"
              << "  --arxiv-profile <DIR>      Enable arXiv WebView session\n"
              << "  --hn-profile <DIR>         Enable Hacker News WebView session\n"
              << "  --pkg-profile <DIR>        Enable npm/PyPI WebView session\n"
              << "  --pwc-profile <DIR>        Enable Papers with Code WebView session\n"
              << "  --hf-profile <DIR>         Enable Hugging Face WebView session\n"
              << "  --s2-profile <DIR>         Enable Semantic Scholar WebView session\n"
              << "  --so-profile <DIR>         Enable Stack Overflow WebView session\n"
              << "  --help                     Show this help\n\n"
              << "HTTP endpoints:\n"
              << "  POST /mcp        JSON-RPC 2.0\n"
              << "  GET  /           Service status (shows enabled sources)\n"
              << "  GET  /tools      List all registered tools (49 total)\n\n"
              << "Full example (all 8 sources):\n"
              << "  research-mcp.exe --port 8765 \\\n"
              << "    --gh-profile ./profiles/gh \\\n"
              << "    --arxiv-profile ./profiles/arxiv \\\n"
              << "    --hn-profile ./profiles/hn \\\n"
              << "    --pkg-profile ./profiles/pkg \\\n"
              << "    --pwc-profile ./profiles/pwc \\\n"
              << "    --hf-profile ./profiles/hf \\\n"
              << "    --s2-profile ./profiles/s2 \\\n"
              << "    --so-profile ./profiles/so \\\n"
              << "    --proxy http://127.0.0.1:7897\n\n"
              << "Minimal (GitHub only, 13 tools):\n"
              << "  research-mcp.exe --port 8765\n\n"
              << "Environment:\n"
              << "  GITHUB_TOKEN             Personal access token (optional)\n"
              << "  GITHUB_RESEARCH_TIMEOUT  Request timeout seconds (default: 30)\n"
              << "  HTTPS_PROXY/HTTP_PROXY/ALL_PROXY  Proxy URL\n";
}

static std::string read_proxy_from_env() {
    const char* keys[] = {
        "HTTPS_PROXY", "https_proxy",
        "HTTP_PROXY", "http_proxy",
        "ALL_PROXY", "all_proxy",
        nullptr
    };
    for (int i = 0; keys[i]; ++i) {
        const char* v = std::getenv(keys[i]);
        if (v && v[0]) return std::string(v);
    }
    return std::string();
}

static std::wstring s2w(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// 每个 --xxx-profile 参数的解析结果
struct ProfileArgs {
    std::string gh, arxiv, hn, pkg, pwc, hf, s2, so;
};

int main(int argc, char* argv[]) {
    const char* token_env = std::getenv("GITHUB_TOKEN");
    const char* timeout_env = std::getenv("GITHUB_RESEARCH_TIMEOUT");
    int timeout = timeout_env ? std::atoi(timeout_env) : 30;
    std::optional<std::string> token;
    if (token_env && token_env[0]) token = std::string(token_env);

    int port = 0;
    std::string proxy_url;
    bool proxy_explicit = false;
    ProfileArgs profiles;

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
            ++i;
        } else if (arg == "--proxy" && i + 1 < argc) {
            proxy_url = argv[++i];
            proxy_explicit = true;
        } else if (arg == "--gh-profile" && i + 1 < argc) {
            profiles.gh = argv[++i];
        } else if (arg == "--arxiv-profile" && i + 1 < argc) {
            profiles.arxiv = argv[++i];
        } else if (arg == "--hn-profile" && i + 1 < argc) {
            profiles.hn = argv[++i];
        } else if (arg == "--pkg-profile" && i + 1 < argc) {
            profiles.pkg = argv[++i];
        } else if (arg == "--pwc-profile" && i + 1 < argc) {
            profiles.pwc = argv[++i];
        } else if (arg == "--hf-profile" && i + 1 < argc) {
            profiles.hf = argv[++i];
        } else if (arg == "--s2-profile" && i + 1 < argc) {
            profiles.s2 = argv[++i];
        } else if (arg == "--so-profile" && i + 1 < argc) {
            profiles.so = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n\n";
            print_help();
            return 1;
        }
    }

    if (!proxy_explicit) proxy_url = read_proxy_from_env();

    github_research::McpServer server(token, timeout);

    if (!proxy_url.empty()) {
        std::cerr << "[mcp] proxy: " << proxy_url << std::endl;
        server.set_proxy(proxy_url);
    } else {
        std::cerr << "[mcp] proxy: none (direct)" << std::endl;
    }

    // GitHub 后端独立 user data dir(8 源隔离)
    if (!profiles.gh.empty()) {
        std::cerr << "[mcp] init GitHub profile: " << profiles.gh << std::endl;
        server.init_github_profile(profiles.gh);
    }

    // 各源 WebView 会话按需初始化
    auto tryInit = [&](const std::string& name, const std::string& path,
                       auto initFn) {
        if (path.empty()) return;
        std::cerr << "[mcp] init " << name << " session: " << path << std::endl;
        if (!initFn(s2w(path), proxy_url)) {
            std::cerr << "[mcp] WARNING: " << name << " session init FAILED"
                      << std::endl;
        }
    };

    tryInit("arXiv",  profiles.arxiv, [&](auto d, auto p) { return server.init_arxiv(d, p); });
    tryInit("HN",     profiles.hn,    [&](auto d, auto p) { return server.init_hackernews(d, p); });
    tryInit("Package",profiles.pkg,   [&](auto d, auto p) { return server.init_package(d, p); });
    tryInit("PWC",    profiles.pwc,   [&](auto d, auto p) { return server.init_paperswithcode(d, p); });
    tryInit("HF",     profiles.hf,    [&](auto d, auto p) { return server.init_huggingface(d, p); });
    tryInit("S2",     profiles.s2,    [&](auto d, auto p) { return server.init_semanticscholar(d, p); });
    tryInit("SO",     profiles.so,    [&](auto d, auto p) { return server.init_stackoverflow(d, p); });

    if (port > 0) return server.run_http(port);
    return server.run();
}
