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

    // 辅助:取 key 对应的 json 值,不存在则返回 json::value_t::null
    // 避免 obj.value(key, nullptr) 在 key 存在且值为 string 时
    // 触发 nlohmann/json type_error.302(get<std::nullptr_t>() 失败)
    auto get_or_null = [](const json& obj, const char* key) -> json {
        if (obj.contains(key)) return obj[key];
        return json(nullptr);
    };

    if (info.is_object()) {
        summary["name"] = get_or_null(info, "full_name");
        summary["description"] = get_or_null(info, "description");
        summary["url"] = get_or_null(info, "html_url");
        summary["stars"] = info.value("stargazers_count", 0);
        summary["forks"] = info.value("forks_count", 0);
        summary["open_issues"] = info.value("open_issues_count", 0);
        summary["language"] = get_or_null(info, "language");
        if (info.contains("license") && info["license"].is_object()) {
            summary["license"] = get_or_null(info["license"], "spdx_id");
        } else {
            summary["license"] = nullptr;
        }
        summary["created_at"] = get_or_null(info, "created_at");
        summary["updated_at"] = get_or_null(info, "updated_at");
        summary["pushed_at"] = get_or_null(info, "pushed_at");
        summary["default_branch"] = get_or_null(info, "default_branch");
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
            {"tag", get_or_null(r, "tag_name")},
            {"name", get_or_null(r, "name")},
            {"date", get_or_null(r, "published_at")}
        };
    } else {
        summary["latest_release"] = nullptr;
    }

    return summary;
}

} // namespace github_research
