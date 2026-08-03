#include "github_research/formatters.hpp"
#include <algorithm>

namespace github_research {

// 格式化 tree 为可读文本(树状缩进)
// 对齐 Python 版 format_tree 语义
std::string format_tree(const json& tree_data, int max_depth) {
    if (!tree_data.is_object() || !tree_data.contains("tree") || !tree_data["tree"].is_array()) {
        return "[Unable to parse tree]";
    }

    std::vector<std::string> lines;
    for (const auto& item : tree_data["tree"]) {
        if (!item.contains("path") || !item["path"].is_string()) continue;
        std::string path = item["path"].get<std::string>();

        // 计算深度(斜杠数)
        int depth = 0;
        for (char c : path) if (c == '/') ++depth;

        if (depth < max_depth) {
            std::string indent(depth * 2, ' ');
            // 取最后一段作为 name
            size_t last_slash = path.find_last_of('/');
            std::string name = (last_slash == std::string::npos) ? path : path.substr(last_slash + 1);

            std::string type = item.value("type", std::string());
            if (type == "tree") {
                lines.push_back(indent + name + "/");
            } else {
                lines.push_back(indent + name);
            }
        }
    }

    // 限制输出 100 行(对齐 Python 版 lines[:100])
    if (lines.size() > 100) {
        lines.resize(100);
    }

    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += "\n";
        result += lines[i];
    }
    return result;
}

// summarize_repo 的纯聚合函数(已由 GitHubClient::summarize_repo 内联实现)
// 此函数保留作为可测试的纯函数接口
json summarize_repo(const json& info,
                    const json& languages,
                    const json& contributors,
                    const json& latest_release) {
    json summary = json::object();

    if (info.is_object()) {
        summary["name"] = info.value("full_name", nullptr);
        summary["description"] = info.value("description", nullptr);
        summary["url"] = info.value("html_url", nullptr);
        summary["stars"] = info.value("stargazers_count", 0);
        summary["forks"] = info.value("forks_count", 0);
        summary["open_issues"] = info.value("open_issues_count", 0);
        summary["language"] = info.value("language", nullptr);
        if (info.contains("license") && info["license"].is_object()) {
            summary["license"] = info["license"].value("spdx_id", nullptr);
        } else {
            summary["license"] = nullptr;
        }
        summary["created_at"] = info.value("created_at", nullptr);
        summary["updated_at"] = info.value("updated_at", nullptr);
        summary["pushed_at"] = info.value("pushed_at", nullptr);
        summary["default_branch"] = info.value("default_branch", nullptr);
        if (info.contains("topics") && info["topics"].is_array()) {
            summary["topics"] = info["topics"];
        } else {
            summary["topics"] = json::array();
        }
    }

    summary["languages"] = languages.is_object() ? languages : json::object();
    if (contributors.is_array()) {
        summary["contributor_count"] = static_cast<int>(contributors.size());
    } else {
        summary["contributor_count"] = "N/A";
    }

    if (latest_release.is_array() && !latest_release.empty()) {
        json r = latest_release[0];
        summary["latest_release"] = {
            {"tag", r.value("tag_name", nullptr)},
            {"name", r.value("name", nullptr)},
            {"date", r.value("published_at", nullptr)}
        };
    } else {
        summary["latest_release"] = nullptr;
    }

    return summary;
}

} // namespace github_research
