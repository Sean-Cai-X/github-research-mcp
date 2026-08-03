#include "github_research/github_client.hpp"
#include "github_research/errors.hpp"
#include <string>

namespace github_research {

// === 辅助:从 arguments 中安全读取字符串参数 ===
static std::string get_string_arg(const json& args, const std::string& key,
                                   const std::string& tool_name, bool required,
                                   std::string& err) {
    if (!args.contains(key)) {
        if (required) {
            err = "missing required parameter: " + key;
        }
        return "";
    }
    const json& v = args[key];
    if (!v.is_string()) {
        err = "invalid type: " + key + " must be string";
        return "";
    }
    std::string s = v.get<std::string>();
    if (s.size() > 256) {
        err = "parameter too long: " + key + " exceeds 256 chars";
        return "";
    }
    return s;
}

// === 辅助:从 arguments 中安全读取整数参数 ===
static int get_int_arg(const json& args, const std::string& key,
                       int default_val, int min_val, int max_val,
                       std::string& err) {
    if (!args.contains(key)) return default_val;
    const json& v = args[key];
    if (!v.is_number_integer()) {
        err = "invalid type: " + key + " must be integer";
        return 0;
    }
    int n = v.get<int>();
    if (n < min_val || n > max_val) {
        err = "out of range: " + key + " must be in [" +
              std::to_string(min_val) + "," + std::to_string(max_val) + "]";
        return 0;
    }
    return n;
}

// === 辅助:构建错误响应 ===
static json make_error_result(const std::string& error_msg) {
    return {
        {"content", json::array({{{"type", "text"}, {"text", error_msg}}})},
        {"isError", true}
    };
}

// === 辅助:构建成功响应 ===
static json make_success_result(const std::string& text) {
    return {
        {"content", json::array({{{"type", "text"}, {"text", text}}})},
        {"isError", false}
    };
}

// === 辅助:构建错误响应(JSON detail) ===
static json make_error_json_result(const std::string& error_msg,
                                    int status_code,
                                    const std::string& url = "",
                                    const std::string& reset_at = "") {
    json detail = {
        {"error", error_msg},
        {"status_code", status_code}
    };
    if (!url.empty()) detail["url"] = url;
    if (!reset_at.empty()) detail["reset_at"] = reset_at;
    return make_error_result(detail.dump());
}

// === 主分发函数 ===
json dispatch_tool_call(GitHubClient& client, const json& params) {
    // 校验 params 结构
    if (!params.is_object()) {
        return make_error_result("invalid params: not an object");
    }

    std::string tool_name = params.value("name", std::string());
    if (tool_name.empty()) {
        return make_error_result("missing required parameter: name");
    }

    json args = params.value("arguments", json::object());
    if (!args.is_object()) {
        return make_error_result("invalid arguments: not an object");
    }

    std::string err;
    std::string owner, repo;

    // 所有 tool 都需要 owner 和 repo
    owner = get_string_arg(args, "owner", tool_name, true, err);
    if (!err.empty()) return make_error_result(err);
    repo = get_string_arg(args, "repo", tool_name, true, err);
    if (!err.empty()) return make_error_result(err);

    try {
        if (tool_name == "github_get_repo_info") {
            json r = client.get_repo_info(owner, repo);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_readme") {
            std::string r = client.get_readme(owner, repo);
            return make_success_result(r);

        } else if (tool_name == "github_get_tree") {
            std::string branch = get_string_arg(args, "branch", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (branch.empty()) branch = "main";

            int max_depth = get_int_arg(args, "max_depth", 3, 1, 10, err);
            if (!err.empty()) return make_error_result(err);

            bool recursive = true;
            if (args.contains("recursive") && args["recursive"].is_boolean()) {
                recursive = args["recursive"].get<bool>();
            }

            std::string r = client.get_tree(owner, repo, branch, max_depth, recursive);
            return make_success_result(r);

        } else if (tool_name == "github_get_languages") {
            json r = client.get_languages(owner, repo);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_contributors") {
            int limit = get_int_arg(args, "limit", 30, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.get_contributors(owner, repo, limit);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_commits") {
            int limit = get_int_arg(args, "limit", 50, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            std::optional<std::string> since;
            if (args.contains("since") && args["since"].is_string()) {
                since = args["since"].get<std::string>();
            }
            json r = client.get_recent_commits(owner, repo, limit, since);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_issues") {
            std::string state = get_string_arg(args, "state", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (state.empty()) state = "all";
            if (state != "open" && state != "closed" && state != "all") {
                return make_error_result("invalid value: state must be open/closed/all");
            }
            int limit = get_int_arg(args, "limit", 30, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            std::optional<std::string> labels;
            if (args.contains("labels") && args["labels"].is_string()) {
                labels = args["labels"].get<std::string>();
            }
            json r = client.get_issues(owner, repo, state, limit, labels);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_pull_requests") {
            std::string state = get_string_arg(args, "state", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (state.empty()) state = "all";
            if (state != "open" && state != "closed" && state != "all") {
                return make_error_result("invalid value: state must be open/closed/all");
            }
            int limit = get_int_arg(args, "limit", 30, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.get_pull_requests(owner, repo, state, limit);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_releases") {
            int limit = get_int_arg(args, "limit", 10, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.get_releases(owner, repo, limit);
            return make_success_result(r.dump());

        } else if (tool_name == "github_summarize_repo") {
            json r = client.summarize_repo(owner, repo);
            return make_success_result(r.dump());

        } else {
            return make_error_result("unknown tool: " + tool_name);
        }

    } catch (const GitHubRateLimitError& e) {
        return make_error_json_result(e.what(), 429, "", e.reset_at());
    } catch (const GitHubAPIError& e) {
        return make_error_json_result(e.what(), e.status_code(), e.url(), "");
    } catch (const std::exception& e) {
        return make_error_result(std::string("internal error: ") + e.what());
    }
}

} // namespace github_research
