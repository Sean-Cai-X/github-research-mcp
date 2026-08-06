#include "github_research/source_fusion.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <chrono>
#include <set>

namespace github_research {

// =============================================================
// 默认字段融合策略
// =============================================================
std::map<std::string, FusionStrategy> default_field_strategies() {
    return {
        {"description",     FusionStrategy::PRIORITY},
        {"stars",           FusionStrategy::LATEST},
        {"forks",           FusionStrategy::LATEST},
        {"authors",         FusionStrategy::UNION},
        {"tech_stack",      FusionStrategy::UNION},
        {"topics",          FusionStrategy::UNION},
        {"dependencies",    FusionStrategy::UNION},
        {"license",         FusionStrategy::PRIORITY},
        {"created_at",      FusionStrategy::PRIORITY},
        {"updated_at",      FusionStrategy::LATEST},
        {"readme_text",     FusionStrategy::PRIORITY},
        {"canonical_name",  FusionStrategy::PRIORITY},
        {"owner",           FusionStrategy::PRIORITY},
        {"primary_language",FusionStrategy::PRIORITY},
        {"latest_version",  FusionStrategy::LATEST},
        {"abstract",        FusionStrategy::PRIORITY},
        {"hn_score",        FusionStrategy::LATEST},
        {"comment_count",   FusionStrategy::LATEST},
    };
}

// =============================================================
// 数据源动态优先级计算
// =============================================================
double calculate_source_priority(const DataSource& ds,
                                  const std::string& entity_type) {
    double base = ds.priority_weight;
    double reliability = ds.reliability;
    double latency_penalty = (ds.avg_latency_ms / 5000.0) * 0.1;
    double failure_penalty = std::min(ds.consecutive_failures * 0.15, 0.6);

    // 实体类型适配
    double type_bonus = 0.0;
    if (entity_type == "project" && ds.source_id == "github_api") type_bonus = 0.3;
    else if (entity_type == "paper" && ds.source_id == "arxiv_api") type_bonus = 0.3;
    else if (entity_type == "person" && ds.source_id == "github_api") type_bonus = 0.2;
    else if (entity_type == "paper" && ds.source_id == "semantic_scholar") type_bonus = 0.2;

    double score = base * 0.3 + reliability * 0.4 + type_bonus
                 - latency_penalty - failure_penalty;
    return std::max(0.0, std::min(1.0, score));
}

// =============================================================
// EntityData::to_json
// =============================================================
json EntityData::to_json() const {
    json fields_j = json::object();
    for (auto& [name, fv] : fields) {
        fields_j[name] = {
            {"value", fv.value},
            {"source", fv.source_id},
            {"quality", fv.quality_score},
            {"is_primary", fv.is_primary}
        };
    }
    return {
        {"entity_id", entity_id},
        {"entity_type", entity_type},
        {"canonical_name", canonical_name},
        {"fields", fields_j},
        {"sources_used", sources_used},
        {"quality_score", quality_score},
        {"coverage_score", coverage_score},
        {"is_fallback_result", is_fallback_result},
        {"fallback_note", fallback_note},
        {"errors", errors}
    };
}

// =============================================================
// CircuitBreaker
// =============================================================
CircuitBreaker::CircuitBreaker(int failure_threshold, int reset_timeout_sec)
    : failure_threshold_(failure_threshold)
    , reset_timeout_sec_(reset_timeout_sec) {}

bool CircuitBreaker::is_open(const std::string& source_id, int64_t now_ts) {
    auto it = states_.find(source_id);
    if (it == states_.end()) return false;
    auto& st = it->second;
    if (st.state == "open") {
        // 超过重置时间,进入半开状态(允许一次试探)
        if (now_ts - st.last_failure_at > reset_timeout_sec_) {
            st.state = "half_open";
            return false;
        }
        return true;
    }
    return false;
}

void CircuitBreaker::record_failure(const std::string& source_id, int64_t now_ts) {
    auto& st = states_[source_id];
    st.failures += 1;
    st.last_failure_at = now_ts;
    if (st.failures >= failure_threshold_) {
        st.state = "open";
    }
}

void CircuitBreaker::record_success(const std::string& source_id) {
    auto it = states_.find(source_id);
    if (it != states_.end()) {
        it->second.failures = 0;
        it->second.state = "closed";
    }
}

std::string CircuitBreaker::get_state(const std::string& source_id) const {
    auto it = states_.find(source_id);
    if (it == states_.end()) return "closed";
    return it->second.state;
}

json CircuitBreaker::dump_all() const {
    json arr = json::array();
    for (auto& [sid, st] : states_) {
        arr.push_back({
            {"source_id", sid},
            {"failures", st.failures},
            {"last_failure_at", st.last_failure_at},
            {"state", st.state}
        });
    }
    return arr;
}

// =============================================================
// CleanerPipeline
// =============================================================
const std::map<std::string, std::string>& CleanerPipeline::field_mapping() {
    static const std::map<std::string, std::string> m = {
        // GitHub
        {"stargazers_count", "stars"},
        {"forks_count", "forks"},
        {"full_name", "canonical_name"},
        {"description", "description"},
        {"language", "primary_language"},
        {"topics", "topics"},
        {"created_at", "created_at"},
        {"updated_at", "updated_at"},
        {"license", "license"},
        {"owner", "owner"},
        // npm
        {"name", "canonical_name"},
        {"version", "latest_version"},
        {"dependencies", "dependencies"},
        {"maintainers", "maintainers"},
        // arXiv
        {"title", "canonical_name"},
        {"summary", "abstract"},
        {"authors", "authors"},
        {"published", "published_at"},
        {"updated", "updated_at"},
        {"primary_category", "primary_category"},
        {"categories", "categories"},
        // HN
        {"score", "hn_score"},
        {"descendants", "comment_count"},
        {"by", "submitter"},
        {"time", "posted_at"},
        {"url", "external_url"},
        {"text", "body_text"},
    };
    return m;
}

std::string CleanerPipeline::strip_html(const std::string& html) {
    std::string out;
    out.reserve(html.size());
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') { in_tag = true; continue; }
        if (c == '>') { in_tag = false; continue; }
        if (!in_tag) out += c;
    }
    return trim_compress(out);
}

std::string CleanerPipeline::trim_compress(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool prev_space = false;
    // skip leading whitespace
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) ++start;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (std::isspace((unsigned char)c)) {
            if (!prev_space) { out += ' '; prev_space = true; }
        } else {
            out += c;
            prev_space = false;
        }
    }
    // trim trailing
    while (!out.empty() && std::isspace((unsigned char)out.back())) out.pop_back();
    return out;
}

double CleanerPipeline::calc_coverage(const std::map<std::string, json>& fields,
                                       const std::string& entity_type) {
    // 期望字段集(按实体类型)
    std::vector<std::string> expected;
    if (entity_type == "project") {
        expected = {"canonical_name","description","stars","forks","owner",
                     "primary_language","topics","license","created_at","updated_at"};
    } else if (entity_type == "paper") {
        expected = {"canonical_name","abstract","authors","published_at",
                     "updated_at","primary_category","categories"};
    } else if (entity_type == "person") {
        expected = {"canonical_name","description"};
    } else {
        expected = {"canonical_name","description"};
    }
    if (expected.empty()) return 1.0;
    int found = 0;
    for (auto& e : expected) {
        if (fields.count(e) && !fields.at(e).is_null()) ++found;
    }
    return (double)found / (double)expected.size();
}

std::map<std::string, json> CleanerPipeline::clean(const json& raw,
                                                     const std::string& source_id) const {
    std::map<std::string, json> out;
    if (!raw.is_object()) return out;

    const auto& mapping = field_mapping();
    for (auto it = raw.begin(); it != raw.end(); ++it) {
        const std::string& key = it.key();
        const json& val = it.value();

        // 查映射
        std::string std_name = key;
        auto mit = mapping.find(key);
        if (mit != mapping.end()) std_name = mit->second;

        // 字符串值: 去 HTML / 压缩空白
        if (val.is_string()) {
            std::string s = val.get<std::string>();
            // 简单检测是否含 HTML 标签
            if (s.find('<') != std::string::npos && s.find('>') != std::string::npos) {
                s = strip_html(s);
            } else {
                s = trim_compress(s);
            }
            if (!s.empty()) out[std_name] = s;
        } else {
            out[std_name] = val;
        }
    }
    return out;
}

// =============================================================
// SourceFusionEngine
// =============================================================
FieldValue SourceFusionEngine::fuse_field(const std::string& field_name,
                                            const FieldValue& existing,
                                            const FieldValue& incoming,
                                            FusionStrategy strategy) {
    FieldValue result = existing;

    switch (strategy) {
        case FusionStrategy::PRIORITY:
            // 保持 existing(优先级更高)
            break;
        case FusionStrategy::LATEST:
            if (incoming.last_updated > existing.last_updated) {
                result = incoming;
            }
            break;
        case FusionStrategy::HIGHEST_QUALITY:
            if (incoming.quality_score > existing.quality_score) {
                result = incoming;
            }
            break;
        case FusionStrategy::NUMERIC_AVG: {
            // 数值取平均
            if (existing.value.is_number() && incoming.value.is_number()) {
                double avg = existing.value.get<double>() * 0.5
                           + incoming.value.get<double>() * 0.5;
                result.value = avg;
                result.source_id = existing.source_id + "+" + incoming.source_id;
            }
            break;
        }
        case FusionStrategy::UNION: {
            // 集合并集(适用于列表)
            json merged = json::array();
            std::set<std::string> seen;
            auto add_items = [&](const json& arr) {
                if (arr.is_array()) {
                    for (auto& item : arr) {
                        std::string s = item.is_string() ? item.get<std::string>()
                                       : item.dump();
                        if (!seen.count(s)) {
                            seen.insert(s);
                            merged.push_back(item);
                        }
                    }
                }
            };
            add_items(existing.value);
            add_items(incoming.value);
            result.value = merged;
            result.source_id = existing.source_id + "+" + incoming.source_id;
            break;
        }
        case FusionStrategy::CONCAT: {
            // 文本拼接
            std::string a = existing.value.is_string() ? existing.value.get<std::string>() : existing.value.dump();
            std::string b = incoming.value.is_string() ? incoming.value.get<std::string>() : incoming.value.dump();
            if (a.find(b) == std::string::npos && b.find(a) == std::string::npos) {
                result.value = a + "\n---\n" + b;
            }
            break;
        }
        case FusionStrategy::MAJORITY:
            // 简化: 保持 existing(多数投票需要 >=3 源才有意义)
            break;
    }
    return result;
}

void SourceFusionEngine::merge_fields(std::map<std::string, FieldValue>& collected,
                                       const std::map<std::string, json>& incoming_fields,
                                       const std::string& source_id,
                                       double source_quality,
                                       int64_t now_ts) {
    auto strategies = default_field_strategies();
    for (auto& [name, val] : incoming_fields) {
        FieldValue fv;
        fv.field_name = name;
        fv.value = val;
        fv.source_id = source_id;
        fv.quality_score = source_quality;
        fv.last_updated = now_ts;
        fv.is_primary = false;
        fv.field_type = val.is_string() ? "string"
                       : val.is_number() ? "number"
                       : val.is_boolean() ? "bool"
                       : val.is_array() ? "list"
                       : "json";

        auto it = collected.find(name);
        if (it == collected.end()) {
            // 新字段,直接加入
            fv.is_primary = true;
            collected[name] = fv;
        } else {
            // 已有,按策略融合
            auto sit = strategies.find(name);
            FusionStrategy strat = (sit != strategies.end()) ? sit->second : FusionStrategy::PRIORITY;
            FieldValue merged = fuse_field(name, it->second, fv, strat);
            merged.is_primary = true;
            collected[name] = merged;
        }
    }
}

double SourceFusionEngine::calc_overall_quality(const std::map<std::string, FieldValue>& fields) {
    if (fields.empty()) return 0.0;
    double sum = 0.0;
    for (auto& [name, fv] : fields) {
        sum += fv.quality_score;
    }
    return sum / (double)fields.size();
}

double SourceFusionEngine::calc_coverage(const std::map<std::string, FieldValue>& fields,
                                          const std::string& entity_type) {
    std::vector<std::string> expected;
    if (entity_type == "project") {
        expected = {"canonical_name","description","stars","forks","owner",
                     "primary_language","topics","license","created_at","updated_at"};
    } else if (entity_type == "paper") {
        expected = {"canonical_name","abstract","authors","published_at",
                     "updated_at","primary_category","categories"};
    } else if (entity_type == "person") {
        expected = {"canonical_name","description"};
    } else {
        expected = {"canonical_name","description"};
    }
    if (expected.empty()) return 1.0;
    int found = 0;
    for (auto& e : expected) {
        if (fields.count(e)) ++found;
    }
    return (double)found / (double)expected.size();
}

} // namespace github_research
