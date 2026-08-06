#pragma once

#include <string>
#include <map>

namespace github_research {

// UTF-8 <-> UTF-16 转换(Windows COM API 需要)
std::wstring to_wstring(const std::string& utf8);
std::string to_utf8(const std::wstring& wide);

// URL 编码(application/x-www-form-urlencoded 风格,但保留 - _ . ~)
std::string url_encode(const std::string& value);

// 字符串小写转换
std::string to_lower(const std::string& s);

// 字符串比较(大小写不敏感)
bool iequals(const std::string& a, const std::string& b);

// 拼接 query string,params 已 URL 编码
std::string build_query(const std::map<std::string, std::string>& params);

// Base64 解码(用于 GitHub Contents API 返回的 base64 编码文件内容)
std::string base64_decode(const std::string& encoded);

} // namespace github_research
