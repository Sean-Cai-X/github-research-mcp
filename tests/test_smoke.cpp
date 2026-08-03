// 最小 smoke test
// 验证:WebView2 初始化 + 真实 GitHub API 调用
// 运行前需设置 GITHUB_TOKEN(可选,无 token 走匿名限流)

#include "github_research/github_client.hpp"
#include "github_research/string_utils.hpp"
#include <iostream>
#include <cstdlib>

static int failures = 0;
static int passes = 0;

#define CHECK(cond, msg) do { \
    if (cond) { passes++; std::cout << "[PASS] " << msg << std::endl; } \
    else { failures++; std::cout << "[FAIL] " << msg << std::endl; } \
} while(0)

int main() {
    using namespace github_research;

    // === 1. 字符串工具测试 ===
    CHECK(url_encode("hello world") == "hello%20world", "url_encode space");
    CHECK(url_encode("a/b") == "a%2Fb", "url_encode slash");
    CHECK(url_encode("a-b_c.d~e") == "a-b_c.d~e", "url_encode safe chars");
    CHECK(to_lower("Hello") == "hello", "to_lower");
    CHECK(iequals("Hello", "HELLO"), "iequals");

    // === 2. JSON-RPC 参数校验测试(不依赖 WebView2) ===
    // 模拟 dispatch_tool_call 的参数校验逻辑
    // 这里只测字符串工具,完整 tool 测试需要 WebView2 环境
    std::cout << "\n--- String utils tests done ---\n" << std::endl;

    // === 3. WebView2 初始化测试(需要 Edge Runtime) ===
    const char* token = std::getenv("GITHUB_TOKEN");
    std::optional<std::string> tok;
    if (token && token[0]) tok = token;

    GitHubClient client(tok, 30);

    // 初始化 WebView2(构造时不初始化,首次请求时初始化)
    // 这里直接调用一个真实 API 来触发初始化
    std::cout << "Initializing WebView2 and calling GitHub API..." << std::endl;

    try {
        // 测试不存在的 repo -> 应该返回 404
        json r = client.get_repo_info("nonexistent-xyz-123", "nonexistent-repo-456");
        // 如果没抛异常,说明返回了 200(不应该发生)
        CHECK(false, "nonexistent repo should return 404");
    } catch (const GitHubAPIError& e) {
        CHECK(e.status_code() == 404, "nonexistent repo returns 404");
        std::cout << "  status: " << e.status_code() << ", msg: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        CHECK(false, std::string("unexpected exception: ") + e.what());
    }

    // === 4. 真实 repo 测试 ===
    try {
        json r = client.summarize_repo("bytedance", "deer-flow");
        CHECK(r.contains("name"), "summarize_repo returns name field");
        CHECK(r.contains("stars"), "summarize_repo returns stars field");
        CHECK(r.contains("forks"), "summarize_repo returns forks field");
        std::cout << "  name: " << r.value("name", "N/A") << std::endl;
        std::cout << "  stars: " << r.value("stars", -1) << std::endl;
    } catch (const std::exception& e) {
        CHECK(false, std::string("real repo test failed: ") + e.what());
    }

    std::cout << "\n=== Results: " << passes << " passed, " << failures << " failed ===" << std::endl;
    return failures == 0 ? 0 : 1;
}
