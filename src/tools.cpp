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

    // github_search_* 工具只需要 query,不需要 owner/repo
    bool is_search = (tool_name == "github_search_repositories" ||
                      tool_name == "github_search_users" ||
                      tool_name == "github_search_index");

    if (!is_search) {
        // 所有非搜索 tool 都需要 owner 和 repo
        owner = get_string_arg(args, "owner", tool_name, true, err);
        if (!err.empty()) return make_error_result(err);
        repo = get_string_arg(args, "repo", tool_name, true, err);
        if (!err.empty()) return make_error_result(err);
    }

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
            // branch / sha: 优先 sha,其次 branch;允许传 "codex/cxcore-integration" 这样的分支名
            std::optional<std::string> sha;
            if (args.contains("sha") && args["sha"].is_string() && !args["sha"].get_ref<const std::string&>().empty()) {
                sha = args["sha"].get<std::string>();
            } else if (args.contains("branch") && args["branch"].is_string() && !args["branch"].get_ref<const std::string&>().empty()) {
                sha = args["branch"].get<std::string>();
            }
            json r = client.get_recent_commits(owner, repo, limit, since, sha);
            return make_success_result(r.dump());

        } else if (tool_name == "github_get_branches") {
            int limit = get_int_arg(args, "limit", 100, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.get_branches(owner, repo, limit);
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

        } else if (tool_name == "github_search_repositories") {
            // 必填 query,可选 sort / order / page
            std::string query = get_string_arg(args, "q", tool_name, true, err);
            if (!err.empty()) return make_error_result(err);
            std::string sort = get_string_arg(args, "sort", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            std::string order = get_string_arg(args, "order", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (order.empty()) order = "desc";
            if (order != "asc" && order != "desc") {
                return make_error_result("invalid value: order must be asc or desc");
            }
            int limit = get_int_arg(args, "limit", 30, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            int page = get_int_arg(args, "page", 1, 1, 10, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.search_repositories(query, sort, order, limit, page);
            return make_success_result(r.dump());

        } else if (tool_name == "github_search_users") {
            std::string query = get_string_arg(args, "q", tool_name, true, err);
            if (!err.empty()) return make_error_result(err);
            std::string sort = get_string_arg(args, "sort", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            std::string order = get_string_arg(args, "order", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (order.empty()) order = "desc";
            if (order != "asc" && order != "desc") {
                return make_error_result("invalid value: order must be asc or desc");
            }
            int limit = get_int_arg(args, "limit", 30, 1, 100, err);
            if (!err.empty()) return make_error_result(err);
            int page = get_int_arg(args, "page", 1, 1, 10, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.search_users(query, sort, order, limit, page);
            return make_success_result(r.dump());

        } else if (tool_name == "github_search_index") {
            // 分层渐进挖掘 - 第一层:轻量索引
            std::string query = get_string_arg(args, "query", tool_name, true, err);
            if (!err.empty()) return make_error_result(err);
            std::string language = get_string_arg(args, "language", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            std::string sort = get_string_arg(args, "sort", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (sort.empty()) sort = "stars";
            int max_results = get_int_arg(args, "max_results", 20, 1, 50, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.search_index(query, language, sort, max_results);
            return make_success_result(r.dump());

        } else if (tool_name == "github_fetch_repo_detail") {
            // 分层渐进挖掘 - 第二层:单仓库深度挖掘
            bool fetch_tech_stack = true;
            bool fetch_code_structure = true;
            bool fetch_top_contributors = true;
            bool fetch_dependencies = true;
            if (args.contains("fetch_tech_stack") && args["fetch_tech_stack"].is_boolean())
                fetch_tech_stack = args["fetch_tech_stack"].get<bool>();
            if (args.contains("fetch_code_structure") && args["fetch_code_structure"].is_boolean())
                fetch_code_structure = args["fetch_code_structure"].get<bool>();
            if (args.contains("fetch_top_contributors") && args["fetch_top_contributors"].is_boolean())
                fetch_top_contributors = args["fetch_top_contributors"].get<bool>();
            if (args.contains("fetch_dependencies") && args["fetch_dependencies"].is_boolean())
                fetch_dependencies = args["fetch_dependencies"].get<bool>();
            int max_contributors = get_int_arg(args, "max_contributors", 15, 1, 30, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.fetch_repo_detail(owner, repo, fetch_tech_stack,
                                              fetch_code_structure, fetch_top_contributors,
                                              fetch_dependencies, max_contributors);
            return make_success_result(r.dump());

        } else if (tool_name == "github_fetch_relation_network") {
            // 分层渐进挖掘 - 第三层:二级递进关联网络
            bool find_similar = true;
            bool similar_by_tech = true;
            bool similar_by_topic = true;
            bool explore_dev = true;
            if (args.contains("find_similar_repos") && args["find_similar_repos"].is_boolean())
                find_similar = args["find_similar_repos"].get<bool>();
            if (args.contains("similar_by_tech_stack") && args["similar_by_tech_stack"].is_boolean())
                similar_by_tech = args["similar_by_tech_stack"].get<bool>();
            if (args.contains("similar_by_topic") && args["similar_by_topic"].is_boolean())
                similar_by_topic = args["similar_by_topic"].get<bool>();
            if (args.contains("explore_developer_links") && args["explore_developer_links"].is_boolean())
                explore_dev = args["explore_developer_links"].get<bool>();
            int max_similar = get_int_arg(args, "max_similar", 10, 1, 20, err);
            if (!err.empty()) return make_error_result(err);
            int dev_depth = get_int_arg(args, "developer_depth", 2, 1, 2, err);
            if (!err.empty()) return make_error_result(err);
            json r = client.fetch_relation_network(owner, repo, find_similar,
                                                   similar_by_tech, similar_by_topic,
                                                   max_similar, explore_dev, dev_depth);
            return make_success_result(r.dump());

        } else if (tool_name == "github_ingest_commit_timeline") {
            // 局部对象连续动态分析索引 - 单 commit 文件变更入库
            std::string commit_hash = get_string_arg(args, "commit_hash", tool_name, true, err);
            if (!err.empty()) return make_error_result(err);
            int n = client.ingest_commit_file_timeline(owner, repo, commit_hash);
            json r = {
                {"success", true},
                {"repo_full_name", owner + "/" + repo},
                {"commit_hash", commit_hash},
                {"records_inserted", n}
            };
            return make_success_result(r.dump());

        } else if (tool_name == "github_ingest_recent_commits_timeline") {
            // 局部对象连续动态分析索引 - 批量增量抓取近 N 天 commits
            int since_days = get_int_arg(args, "since_days", 365, 1, 365, err);
            if (!err.empty()) return make_error_result(err);
            std::string branch = get_string_arg(args, "branch", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            int max_commits = get_int_arg(args, "max_commits", 100, 1, 500, err);
            if (!err.empty()) return make_error_result(err);
            int n = client.ingest_recent_commits_timeline(owner, repo, since_days, branch, max_commits);
            json r = {
                {"success", true},
                {"repo_full_name", owner + "/" + repo},
                {"since_days", since_days},
                {"records_inserted", n}
            };
            return make_success_result(r.dump());

        } else if (tool_name == "github_module_timeline_analysis") {
            // 局部对象连续动态分析索引 - 三层职责统一入口
            std::string target_type = get_string_arg(args, "target_type", tool_name, true, err);
            if (!err.empty()) return make_error_result(err);
            if (target_type != "file" && target_type != "module" && target_type != "signature") {
                return make_error_result("invalid target_type: must be file/module/signature");
            }
            std::string target_path = get_string_arg(args, "target_path", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            std::string module_name = get_string_arg(args, "module_name", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            std::string signature_regex = get_string_arg(args, "signature_regex", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            std::string time_range = get_string_arg(args, "time_range", tool_name, false, err);
            if (!err.empty()) return make_error_result(err);
            if (time_range.empty()) time_range = "1y";
            if (time_range != "1y" && time_range != "180d" &&
                time_range != "90d" && time_range != "30d") {
                return make_error_result("invalid time_range: must be 1y/180d/90d/30d");
            }
            int layer = get_int_arg(args, "layer", 2, 1, 3, err);
            if (!err.empty()) return make_error_result(err);
            bool ingest_first = true;
            if (args.contains("ingest_first") && args["ingest_first"].is_boolean()) {
                ingest_first = args["ingest_first"].get<bool>();
            }
            json r = client.module_timeline_analysis(owner, repo, target_type,
                                                      target_path, module_name,
                                                      signature_regex, time_range,
                                                      layer, ingest_first);
            return make_success_result(r.dump(-1, ' ', false, json::error_handler_t::replace));

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
