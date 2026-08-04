# github-research-mcp (DeerFlow++)

8 源统一研究 MCP 服务,基于 **单一 WebView2 技术栈**。

## 特性

- **8 源 49 个工具**:GitHub / arXiv / Hacker News / npm+PyPI / Papers with Code / Hugging Face / Semantic Scholar / Stack Overflow
- **单一技术栈**:一个基类 `WebViewSession`,一种调用模式 `Navigate + ExecuteScript`,一种错误处理范式,一种日志输出格式,一种限流策略
- **无 HTTP API 依赖**:不混入 WinHTTP / libcurl / cpr,避免两套网络层 / 两套错误处理 / 两套限流逻辑 / 两套调试方式
- **统一原始文本提取**:所有工具统一返回 `{success, url, title, text, html}`,DOM 解析交给 AI,工具不做复杂选择器适配
- **多实例会话隔离**:每个数据源独立 `WebViewSession` + 独立 user data dir,避免 Cookie / 缓存共享
- **串行执行**:所有工具调用串行阻塞,无并行 / 线程池 / detach,简单可调试
- **MCP over stdio + HTTP**:JSON-RPC 2.0,兼容 Claude Desktop / llama.app / TRAE / Cursor

## 架构

### 单一技术栈原则

```
┌─────────────────────────────────────────────────────────┐
│                    MCP Server (HTTP/stdio)              │
└────────────┬────────────────────────────────────────────┘
             │ JSON-RPC 2.0 dispatch
             ▼
┌─────────────────────────────────────────────────────────┐
│  dispatch_<source>_tool  (按工具名前缀路由)              │
│  github_* / arxiv_* / hn_* / pkg_* / pwc_* /            │
│  hf_*   / s2_*    / so_*                                │
└────────────┬────────────────────────────────────────────┘
             │
   ┌─────────┼─────────┬─────────┬─────────┬─────────┐
   ▼         ▼         ▼         ▼         ▼         ▼
┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
│GitHub│ │arXiv │ │  HN  │ │ Pkg  │ │ PWC  │ │ ...  │
│Client│ │Session│ │Session│ │Session│ │Session│ │ ...
└──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘ └──┬───┘
   │        │        │        │        │        │
   └────────┴────────┴────────┴────────┴────────┘
                         │
                         ▼
              ┌─────────────────────┐
              │  WebView2 基类       │
              │  Navigate + Execute │
              │  Script             │
              └─────────────────────┘
```

### 调用模式(所有源统一)

1. `Navigate(url)` —— 导航到目标 URL
2. `WaitForNavigation(timeoutMs)` —— 等待 NavigationCompleted(pump 消息循环)
3. `ExecuteScript(kJsExtractRawPage)` —— 执行统一 JS 提取原始页面文本
4. 返回 `{success, url, title, text, html}` 给 MCP 客户端,AI 自行解析

**统一 JS 脚本** `kJsExtractRawPage`(定义在 `webview_helpers.hpp`):

```javascript
(function(){
    var text = document.body ? document.body.innerText : "";
    if(text.length > 50000) text = text.substring(0, 50000);
    var html = document.documentElement.outerHTML;
    if(html.length > 50000) html = html.substring(0, 50000);
    return JSON.stringify({
        success: true,
        url: window.location.href,
        title: document.title || "",
        text: text,
        html: html
    });
})();
```

**设计理念**:工具只负责"取到页面内容",解析交给 AI。避免为每个站点维护复杂 DOM 选择器,降低维护成本。

**不使用** `fetch()` / `XMLHttpRequest`(WebView2 ExecuteScript 不 await Promise,同步 XHR 被 Chromium 限制)。

### 会话隔离

每个数据源独立 `WebViewSession` 实例 + 独立 user data dir:

| 源 | 类 | user data dir 参数 |
|---|---|---|
| GitHub | `WebViewClient`(基于 `WebViewSession`) | `--gh-profile` |
| arXiv | `WebViewSession` | `--arxiv-profile` |
| Hacker News | `WebViewSession` | `--hn-profile` |
| npm/PyPI | `WebViewSession` | `--pkg-profile` |
| Papers with Code | `WebViewSession` | `--pwc-profile` |
| Hugging Face | `WebViewSession` | `--hf-profile` |
| Semantic Scholar | `WebViewSession` | `--s2-profile` |
| Stack Overflow | `WebViewSession` | `--so-profile` |

## 环境要求

- Windows 10/11 x64
- Edge Runtime(Win10/11 自带)
- Visual Studio 2022(C++ 桌面开发 + Windows 11 SDK)
- CMake 3.16+

## 依赖

| 依赖 | 获取方式 |
|---|---|
| WebView2 SDK | NuGet 包 `Microsoft.Web.WebView2`,解压到 `third_party/WebView2/` |
| nlohmann/json | vcpkg 安装,或单 header 放到 `third_party/json/include/` |

## 构建

```powershell
cd D:\DeerFlow\DeerFlow++
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建产物:
- `build/Release/research-mcp.exe`
- `build/Release/WebView2Loader.dll`

## 启动

### 单源模式(仅 GitHub,向后兼容)

```powershell
.\research-mcp.exe --port 9876 --proxy http://127.0.0.1:7897
```

### 8 源全量模式(推荐)

```powershell
.\research-mcp.exe --port 8765 `
  --gh-profile    ./profiles/gh `
  --arxiv-profile ./profiles/arxiv `
  --hn-profile    ./profiles/hn `
  --pkg-profile   ./profiles/pkg `
  --pwc-profile   ./profiles/pwc `
  --hf-profile    ./profiles/hf `
  --s2-profile    ./profiles/s2 `
  --so-profile    ./profiles/so `
  --proxy http://127.0.0.1:7897
```

启动成功日志:

```
[mcp] proxy: http://127.0.0.1:7897
[mcp] init GitHub profile: ./profiles/gh
[mcp] init arXiv session: ./profiles/arxiv
[session] WebView2 ready, profile: ./profiles/arxiv
[mcp] arXiv session ready
[mcp] init HN session: ./profiles/hn
[session] WebView2 ready, profile: ./profiles/hn
[mcp] HackerNews session ready
...
[mcp] init SO session: ./profiles/so
[session] WebView2 ready, profile: ./profiles/so
[mcp] StackOverflow session ready
[mcp] server starting in HTTP mode on port 8765
[http] MCP server listening on http://127.0.0.1:8765/mcp (Ctrl+C to stop)
```

### 命令行参数

| 参数 | 说明 |
|---|---|
| `--port <PORT>` | HTTP MCP server 端口(默认:stdio 模式) |
| `--proxy <URL>` | 代理 URL(应用到所有 WebView 会话) |
| `--gh-profile <DIR>` | GitHub WebView user data dir(8 源隔离) |
| `--arxiv-profile <DIR>` | 启用 arXiv WebView 会话 |
| `--hn-profile <DIR>` | 启用 Hacker News WebView 会话 |
| `--pkg-profile <DIR>` | 启用 npm/PyPI WebView 会话 |
| `--pwc-profile <DIR>` | 启用 Papers with Code WebView 会话 |
| `--hf-profile <DIR>` | 启用 Hugging Face WebView 会话 |
| `--s2-profile <DIR>` | 启用 Semantic Scholar WebView 会话 |
| `--so-profile <DIR>` | 启用 Stack Overflow WebView 会话 |

未指定 `--xxx-profile` 的源不启用,对应工具调用返回 `session not initialized`。

## 代理设置

WebView2 浏览器链路通过 Chromium 内核的 `--proxy-server` 命令行参数支持显式代理。
代理优先级:**命令行 `--proxy`** > 环境变量(`HTTPS_PROXY` > `HTTP_PROXY` > `ALL_PROXY`)。

### 方式 1:命令行参数(推荐)

```powershell
.\research-mcp.exe --port 8765 --proxy http://127.0.0.1:7897
```

### 方式 2:环境变量

```powershell
$env:HTTPS_PROXY = "http://127.0.0.1:7897"
$env:HTTP_PROXY  = "http://127.0.0.1:7897"
.\research-mcp.exe --port 8765
```

### 代理参数格式

| 格式 | 示例 | 说明 |
|---|---|---|
| `http://host:port` | `http://127.0.0.1:7897` | HTTP 代理(自动剥离 `http://` 前缀) |
| `http://user:pass@host:port` | `http://user:pass@proxy.example.com:8080` | 带认证的代理 |
| `socks5://host:port` | `socks5://127.0.0.1:1080` | SOCKS5 代理 |

## 验证调用

### tools/list

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:8765/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}"
```

### arXiv 可用性检查

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:8765/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"arxiv_check_available\",\"arguments\":{}}}"
```

响应:

```json
{
  "id": 2, "jsonrpc": "2.0",
  "result": {
    "content": [{"type":"text","text":"{\"available\":true,\"page_title\":\"arXiv.org e-Print archive\",\"success\":true}"}],
    "isError": false
  }
}
```

### Hacker News 头条

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:8765/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"hn_get_top_stories\",\"arguments\":{\"count\":3}}}"
```

### GitHub 仓库信息

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:8765/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"github_get_repo_info\",\"arguments\":{\"owner\":\"bytedance\",\"repo\":\"deer-flow\"}}}"
```

## 工具列表(49 个)

### GitHub(13 个)

| 工具 | 说明 |
|---|---|
| `github_get_repo_info` | 仓库基础信息 |
| `github_get_readme` | README 全文(markdown) |
| `github_get_tree` | 目录树(格式化文本) |
| `github_get_languages` | 语言分布 |
| `github_get_contributors` | 贡献者列表 |
| `github_get_commits` | 近期提交(支持 `branch` / `sha` 参数查询非默认分支) |
| `github_get_branches` | 全部分支列表(用于按分支汇总提交时间线) |
| `github_get_issues` | Issues 列表 |
| `github_get_pull_requests` | PR 列表 |
| `github_get_releases` | 发布历史 |
| `github_summarize_repo` | 综合摘要 |
| `github_search_repositories` | 按项目名 / 语言 / topic / stars 搜索仓库(trending / discovery) |
| `github_search_users` | 按作者名 / 组织 / 地区 / 粉丝数搜索用户 |

### arXiv(4 个)

| 工具 | 说明 |
|---|---|
| `arxiv_search_papers` | 按关键词 / 分类搜索论文 |
| `arxiv_get_paper_detail` | 获取论文详情(标题、作者、摘要、PDF 链接) |
| `arxiv_get_pdf_link` | 根据 arXiv ID 生成 PDF / abs 链接 |
| `arxiv_check_available` | 检查 arXiv 网站可用性 |

### Hacker News(5 个)

| 工具 | 说明 |
|---|---|
| `hn_get_top_stories` | 头条故事 |
| `hn_get_new_stories` | 最新故事 |
| `hn_get_best_stories` | 精选故事 |
| `hn_get_item` | 获取单个 item(故事 / 评论) |
| `hn_search_by_keyword` | 按关键词搜索 |

### npm / PyPI(4 个)

| 工具 | 说明 |
|---|---|
| `pkg_search_npm` | 搜索 npm 包 |
| `pkg_get_npm_detail` | 获取 npm 包详情 |
| `pkg_search_pypi` | 搜索 PyPI 包 |
| `pkg_get_pypi_detail` | 获取 PyPI 包详情 |

### Papers with Code(5 个)

| 工具 | 说明 |
|---|---|
| `pwc_search_papers` | 搜索论文 |
| `pwc_get_paper_detail` | 论文详情 |
| `pwc_get_sota` | 获取 SOTA(State-of-the-Art)结果 |
| `pwc_search_tasks` | 搜索任务 |
| `pwc_search_datasets` | 搜索数据集 |

### Hugging Face(7 个)

| 工具 | 说明 |
|---|---|
| `hf_search_models` | 搜索模型 |
| `hf_get_model_info` | 模型详情 |
| `hf_get_model_readme` | 模型 README |
| `hf_search_datasets` | 搜索数据集 |
| `hf_get_dataset_info` | 数据集详情 |
| `hf_get_trending_models` | 热门模型 |
| `hf_search_spaces` | 搜索 Spaces |

### Semantic Scholar(6 个)

| 工具 | 说明 |
|---|---|
| `s2_search_papers` | 搜索论文 |
| `s2_get_paper_detail` | 论文详情 |
| `s2_get_citations` | 引用列表 |
| `s2_get_references` | 参考文献列表 |
| `s2_get_author_papers` | 作者论文列表 |
| `s2_search_author` | 搜索作者 |

### Stack Overflow(5 个)

| 工具 | 说明 |
|---|---|
| `so_search_questions` | 搜索问题 |
| `so_get_question_detail` | 问题详情(含答案) |
| `so_get_top_answers` | 获取热门答案 |
| `so_search_by_tags` | 按标签搜索 |
| `so_get_similar` | 获取相似问题 |

## 客户端配置

### Claude Desktop / TRAE

```json
{
  "mcpServers": {
    "github-research": {
      "command": "D:\\DeerFlow\\DeerFlow++\\build\\Release\\research-mcp.exe",
      "args": [
        "--port", "8765",
        "--gh-profile",    "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\gh",
        "--arxiv-profile", "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\arxiv",
        "--hn-profile",    "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\hn",
        "--pkg-profile",   "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\pkg",
        "--pwc-profile",   "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\pwc",
        "--hf-profile",    "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\hf",
        "--s2-profile",    "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\s2",
        "--so-profile",    "D:\\DeerFlow\\DeerFlow++\\build\\Release\\profiles\\so",
        "--proxy", "http://127.0.0.1:7897"
      ],
      "env": {
        "GITHUB_TOKEN": "ghp_xxx"
      }
    }
  }
}
```

### llama.cpp 集成

```powershell
llama-server.exe ^
  -m Qwen2.5-7B-Instruct.Q4_K_M.gguf ^
  --port 8080 ^
  --mcp http://127.0.0.1:8765/mcp
```

挂载后 llama.cpp 自动执行 `initialize` 握手 → `tools/list`,把 49 个工具注册为 `McpServer` tool 的子项,LLM 可通过 `McpServer(name="arxiv_search_papers", arguments={...})` 形式调用。

## 协议兼容性

| 协议项 | 支持情况 | 说明 |
|---|---|---|
| JSON-RPC 2.0 | ✅ | 标准 request/response/notification |
| MCP 版本 | 2024-11-05 | `initialize` 协议握手版本 |
| 传输方式 | stdio + HTTP | HTTP 路径 `/mcp`,Content-Type `application/json` |
| 批量请求 | ✅ | JSON 数组形式的批量 JSON-RPC |
| CORS | ✅ | 响应头 `Access-Control-Allow-Origin: *` |
| OPTIONS 预检 | ✅ | 自动返回 200 |
| `tools/list` | ✅ | 49 个工具(8 源) |
| `tools/call` | ✅ | 支持 `isError` 字段标记失败 |
| `ping` | ✅ | 心跳保活 |
| `shutdown` | ✅ | 触发 server 优雅停止 |

## 错误处理

所有错误返回 `isError=true`,content 为 JSON 字符串:

```json
{"error":"repository not found","status_code":404,"url":"https://api.github.com/repos/..."}
```

GitHub 限流错误附带 `reset_at`:

```json
{"error":"rate limit exceeded","status_code":429,"reset_at":"1785724800"}
```

会话未初始化错误:

```json
{"error":"ERROR: arXiv WebView session not initialized."}
```

## 故障排查

### `WebView2 initialization timeout`

Edge Runtime 缺失或被沙箱阻止。检查:
1. `reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" /v pv` 应返回版本号
2. 在非沙箱环境(真实 Windows 终端)运行
3. 设置 `WEBVIEW2_USER_DATA_DIR` 环境变量到可写目录

### `HTTP 403`(GitHub)

GitHub API 限流(每小时 60 次/未认证)。解决:
1. 设置 `GITHUB_TOKEN` 环境变量(Personal Access Token,每小时 5000 次)
2. 等待限流重置(查看响应头 `X-RateLimit-Reset`)

### `session not initialized`

对应源的 `--xxx-profile` 参数未指定。检查启动命令是否包含全部 8 个 `--xxx-profile` 参数。

### `fetch_result preview: {}`

WebView2 ExecuteScript 不 await Promise 的已知问题。本服务已改用 `Navigate + ExecuteScript` 模式(读取 `document.body.innerText`),不再使用 `fetch()` / `XMLHttpRequest`。如仍出现此错误,请确认使用最新构建的 exe。

## 测试

```powershell
cd D:\DeerFlow\DeerFlow++
cmake --build build --config Release --target test_smoke
.\build\Release\test_smoke.exe
```

## 与 DeerFlow 原版的差异

| 维度 | DeerFlow 原版 | 本地 MCP 版 |
|---|---|---|
| 实现语言 | Python skill + agent runtime | C++17 可执行文件 + LLM 客户端 |
| HTTP 后端 | Python requests | WebView2(Chromium 内核) |
| 浏览器指纹 | 无 | 完整(与 Edge 一致) |
| 反爬能力 | 弱 | 强(真实浏览器) |
| 数据源 | GitHub 单源 | 8 源统一接入 |
| 编排主体 | agent runtime | 本地 LLM 客户端 |
| 平台 | 跨平台 | Windows 10/11 专属 |

## 许可

随 DeerFlow 仓库分发。
