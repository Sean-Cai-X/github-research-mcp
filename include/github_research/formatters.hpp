#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "errors.hpp"

namespace github_research {

using json = nlohmann::json;

// 格式化 tree 为可读文本(树状缩进)
// tree_data: GitHub API /repos/{owner}/{repo}/git/trees 返回的 JSON
// max_depth: 最大缩进深度,超过则不缩进
std::string format_tree(const json& tree_data, int max_depth = 3);

// 聚合 repo info / languages / contributors / latest_release 为 summary
// 每个子调用失败时对应字段置 "N/A" 或 null,不抛异常
json summarize_repo(const json& info,
                    const json& languages,
                    const json& contributors,
                    const json& latest_release);

} // namespace github_research
