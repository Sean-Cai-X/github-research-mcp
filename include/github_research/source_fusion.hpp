#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace github_research {

using json = nlohmann::json;

// =============================================================
// FusionStrategy: 字段级融合策略
// =============================================================
enum class FusionStrategy {
    PRIORITY,          // 取优先级最高源的值(默认)
    MAJORITY,          // 多数投票
    LATEST,            // 取最新更新的
    HIGHEST_QUALITY,   // 取质量分最高的
    CONCAT,            // 拼接合并(文本)
    NUMERIC_AVG,       // 数值取平均
    UNION              // 集合并集(标签/依赖列表)
};

// 字段名 → 融合策略 默认映射
std::map<std::string, FusionStrategy> default_field_strategies();

// =============================================================
// DataSource: 数据源元信息(对应 data_sources 表)
// =============================================================
struct DataSource {
    std::string source_id;        // github_api / arxiv_api / ...
    std::string source_type;      // api / web_scrape / rss / file
    std::string base_url;
    double reliability = 0.8;     // 历史可靠度 0~1
    int avg_latency_ms = 0;
    int rate_limit_per_hour = 0;
    double priority_weight = 1.0;
    bool enabled = true;
    int64_t last_failure_at = 0;
    int consecutive_failures = 0;
    std::string config_json;
};

// 计算数据源对某实体类型的动态优先级 (0~1)
double calculate_source_priority(const DataSource& ds,
                                  const std::string& entity_type);

// =============================================================
// FieldValue: 单个字段的值+来源+质量
// =============================================================
struct FieldValue {
    std::string field_name;       // 标准字段名
    json value;                   // 字段值
    std::string field_type;       // string / number / json / list / bool
    std::string source_id;        // 来自哪个数据源
    double quality_score = 0.5;
    int64_t last_updated = 0;
    bool is_primary = false;
};

// =============================================================
// EntityData: 融合后的实体数据(get_entity 返回值)
// =============================================================
struct EntityData {
    std::string entity_id;
    std::string entity_type;
    std::string canonical_name;
    std::map<std::string, FieldValue> fields;  // field_name → FieldValue
    std::vector<std::string> sources_used;
    double quality_score = 0.0;
    double coverage_score = 0.0;
    bool is_fallback_result = false;
    std::string fallback_note;
    std::vector<std::string> errors;

    // 序列化为 JSON
    json to_json() const;
};

// =============================================================
// CircuitBreaker: 熔断器
// 连续失败达到阈值后暂时熔断,定期半开试探
// =============================================================
class CircuitBreaker {
public:
    CircuitBreaker(int failure_threshold = 5, int reset_timeout_sec = 300);

    // 是否熔断中(返回 true = 跳过该源)
    bool is_open(const std::string& source_id, int64_t now_ts);

    // 记录失败
    void record_failure(const std::string& source_id, int64_t now_ts);

    // 记录成功(重置计数器)
    void record_success(const std::string& source_id);

    // 获取某源的熔断状态
    std::string get_state(const std::string& source_id) const;

    // 获取所有源的熔断状态(JSON)
    json dump_all() const;

private:
    int failure_threshold_;
    int reset_timeout_sec_;
    // source_id → {failures, last_failure_at, state}
    struct BreakerState {
        int failures = 0;
        int64_t last_failure_at = 0;
        std::string state = "closed"; // closed / open / half_open
    };
    std::map<std::string, BreakerState> states_;
};

// =============================================================
// CleanerPipeline: 数据清洗管道
// 字段映射 + 基本清洗(去 HTML 标签 / 去空白 / 格式标准化)
// =============================================================
class CleanerPipeline {
public:
    // 源字段名 → 标准字段名 映射
    static const std::map<std::string, std::string>& field_mapping();

    // 清洗一条原始记录
    // raw: 原始 JSON (来自某数据源)
    // source_id: 数据源标识
    // 返回: 标准化后的 fields map (field_name → value)
    std::map<std::string, json> clean(const json& raw,
                                       const std::string& source_id) const;

    // 去除 HTML 标签,保留纯文本
    static std::string strip_html(const std::string& html);

    // 去除首尾空白 + 压缩连续空白
    static std::string trim_compress(const std::string& s);

    // 计算字段覆盖率 (有值的字段 / 期望字段总数)
    static double calc_coverage(const std::map<std::string, json>& fields,
                                 const std::string& entity_type);
};

// =============================================================
// SourceFusionEngine: 多源字段融合
// =============================================================
class SourceFusionEngine {
public:
    // 融合两个同名字段,返回裁决后的 FieldValue
    static FieldValue fuse_field(const std::string& field_name,
                                  const FieldValue& existing,
                                  const FieldValue& incoming,
                                  FusionStrategy strategy);

    // 批量融合: 将 incoming_fields 合并到 collected 中
    static void merge_fields(std::map<std::string, FieldValue>& collected,
                              const std::map<std::string, json>& incoming_fields,
                              const std::string& source_id,
                              double source_quality,
                              int64_t now_ts);

    // 计算实体整体质量分
    static double calc_overall_quality(const std::map<std::string, FieldValue>& fields);

    // 计算覆盖率
    static double calc_coverage(const std::map<std::string, FieldValue>& fields,
                                 const std::string& entity_type);
};

} // namespace github_research
