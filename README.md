# github-research-mcp (DeerFlow++)

GitHub 深度研究 MCP 服务,基于 WebView2 浏览器链路。

## 特性

- **10 个 GitHub 工具**:repo_info / readme / tree / languages / contributors / commits / issues / pull_requests / releases / summarize_repo
- **WebView2 浏览器链路**:Chromium 内核,完整浏览器指纹(TLS JA3 / HTTP/2 / Headers 顺序),反爬能力强
- **自动系统代理**:继承 Edge/系统代理设置
- **MCP over stdio**:JSON-RPC 2.0,兼容 Claude Desktop / TRAE / Cursor
- **零运行时依赖**:静态链接 CRT,单 exe + WebView2Loader.dll

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

### WebView2 SDK 布局

```
third_party/WebView2/
├── include/
│   ├── WebView2.h
│   ├── WebView2Environment.h
│   └── WebView2Experimental.h(可选)
├── lib/
│   └── x64/
│       └── WebView2Loader.dll.lib
└── redist/
    └── x64/
        └── WebView2Loader.dll
```

下载 NuGet 包后用 7zip 解压,按上述布局放置即可。

## 构建

```powershell
cd D:\DeerFlow\DeerFlow++
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

构建产物:
- `build/Release/github-research-mcp.exe`
- `build/Release/WebView2Loader.dll`

## 配置

复制 `.env.example` 为 `.env` 并填写:

```
GITHUB_TOKEN=your-github-personal-access-token
GITHUB_RESEARCH_TIMEOUT=30
```

或在 MCP 客户端配置中直接设置环境变量(见 `mcp_config_example.json`)。

## 代理设置

WebView2 浏览器链路通过 Chromium 内核的 `--proxy-server` 命令行参数支持显式代理。
代理优先级:**命令行 `--proxy`** > 环境变量(`HTTPS_PROXY` > `HTTP_PROXY` > `ALL_PROXY`)。

### 代理配置链路

| 层级 | 文件 | 职责 |
|---|---|---|
| 入口 | [src/main.cpp](src/main.cpp) | 解析 `--proxy` 参数,读取环境变量 |
| 转发 | [include/github_research/mcp_server.hpp](include/github_research/mcp_server.hpp) | `McpServer::set_proxy` → `GitHubClient::set_proxy` |
| 传递 | [include/github_research/github_client.hpp](include/github_research/github_client.hpp) | `GitHubClient::set_proxy` → `WebViewClient::set_proxy` |
| 应用 | [src/webview_client.cpp](src/webview_client.cpp) | `CoreWebView2EnvironmentOptions` 携带 `--proxy-server` 启动 Chromium |

> 注意:`set_proxy` 必须在 `initialize()` / `run()` / `run_http()` 之前调用,否则 Chromium 进程已启动,代理参数无法注入。

### 使用方式

#### 方式 1:命令行参数(推荐)

```powershell
.\github-research-mcp.exe --port 9876 --proxy http://127.0.0.1:7897
```

#### 方式 2:环境变量

```powershell
$Proxy = "http://127.0.0.1:7897"
$env:HTTPS_PROXY = $Proxy
$env:HTTP_PROXY  = $Proxy
$env:ALL_PROXY   = $Proxy
.\github-research-mcp.exe --port 9876
```

#### 方式 3:MCP 客户端配置(Claude Desktop / TRAE)

在 `claude_desktop_config.json` 中通过 `env` 注入:

```json
{
  "mcpServers": {
    "github-research": {
      "command": "D:\\DeerFlow\\DeerFlow++\\build\\Release\\github-research-mcp.exe",
      "args": ["--proxy", "http://127.0.0.1:7897"],
      "env": {
        "GITHUB_TOKEN": "ghp_xxx"
      }
    }
  }
}
```

或仅使用环境变量(让本服务自动读取):

```json
{
  "mcpServers": {
    "github-research": {
      "command": "D:\\DeerFlow\\DeerFlow++\\build\\Release\\github-research-mcp.exe",
      "env": {
        "GITHUB_TOKEN": "ghp_xxx",
        "HTTPS_PROXY": "http://127.0.0.1:7897",
        "HTTP_PROXY":  "http://127.0.0.1:7897",
        "ALL_PROXY":   "http://127.0.0.1:7897"
      }
    }
  }
}
```

### 代理参数格式

| 格式 | 示例 | 说明 |
|---|---|---|
| `http://host:port` | `http://127.0.0.1:7897` | HTTP 代理(Chromium 会自动去掉 `http://` 前缀) |
| `http://user:pass@host:port` | `http://user:pass@proxy.example.com:8080` | 带认证的代理 |
| `socks5://host:port` | `socks5://127.0.0.1:1080` | SOCKS5 代理 |

> 注意:Chromium 的 `--proxy-server` 参数不接受带 `http://` scheme 前缀的 URL,本服务在 [src/webview_client.cpp](src/webview_client.cpp) 中已自动剥离该前缀,用户可直接使用完整 URL。

### 启动日志验证

代理生效时,stderr 会输出:

```
[mcp] proxy: http://127.0.0.1:7897
[webview] using proxy: http://127.0.0.1:7897
[webview] user data dir: ...
[webview] initialized (origin: https://api.github.com)
```

未设置代理时输出:

```
[mcp] proxy: none (direct connection)
```

### 验证调用

启动后通过 curl 调用任意 GitHub 工具,确认能正常返回数据即代理生效:

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:9876/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"github_get_repo_info\",\"arguments\":{\"owner\":\"Sean-Cai-X\",\"repo\":\"github-research-mcp\"}}}"
```

### WinHTTP 模式(暂未启用)

当前版本仅支持 WebView2 浏览器链路的代理设置。WinHTTP 客户端的代理参数读取已预留接口(`WinHttpClient` 的环境变量读取逻辑),后续启用时无需改动入口与配置链路。

## 客户端配置

参考 `mcp_config_example.json`,在 MCP 客户端(如 Claude Desktop)配置文件中添加 4 个 MCP server:

1. **github-research**(本项目)- GitHub 数据采集
2. **brave-search** - Web 搜索(替代 DeerFlow web_search)
3. **fetch** - URL 抓取(替代 DeerFlow web_fetch)
4. **filesystem** - 写报告文件

将 `system_prompt.md` 内容作为客户端的 system prompt 或 project instructions 注入。

## llama.cpp 集成

llama.cpp server(自 `b4000+` 起)原生支持 MCP,有两种角色,均可与本服务对接。

### 角色 1:llama.cpp 作为 MCP Client(推荐)

llama.cpp server 通过 `--mcp` 参数挂载远端 MCP server,工具自动暴露为内置 `McpServer` tool,LLM 即可在对话中调用。

#### 启动 github-research-mcp

```powershell
# 先启动本服务(HTTP 模式)
cd D:\DeerFlow\DeerFlow++\build\Release
.\github-research-mcp.exe --port 9876
```

#### 启动 llama.cpp server

```powershell
llama-server.exe ^
  -m Qwen2.5-7B-Instruct.Q4_K_M.gguf ^
  --port 8080 ^
  --mcp http://127.0.0.1:9876/mcp
```

参数说明:

| 参数 | 语义 |
|---|---|
| `--mcp <URL>` | 挂载远端 MCP server(Streamable HTTP),URL 指向 `/mcp` 端点 |
| `--mcp-timeout <ms>` | 单次工具调用超时(默认 60000ms) |
| `--mcp-headers` | 自定义请求头(JSON 格式) |
| `--mcp-strict` | 严格模式,工具 schema 校验失败即拒绝 |

挂载后 llama.cpp 自动执行 `initialize` 握手 → `notifications/initialized` → `tools/list`,把 12 个 `github_*` 工具注册为 `McpServer` tool 的子项,LLM 可通过 `McpServer(name="github_get_repo_info", arguments={...})` 形式调用。

#### curl 直接调用语义示例

完整 JSON-RPC 2.0 调用序列(参考用,实际由 llama.cpp 自动完成):

**1. initialize(握手)**

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:9876/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"llama.cpp\",\"version\":\"b4000\"}}}"
```

响应:

```json
{
  "id": 1,
  "jsonrpc": "2.0",
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {"tools": {}},
    "serverInfo": {"name": "github-research-mcp", "version": "0.1.0"}
  }
}
```

**2. notifications/initialized(通知,无响应)**

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:9876/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}"
```

返回 HTTP 202(无 body)。

**3. tools/list(枚举工具)**

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:9876/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}"
```

响应(节选):

```json
{
  "id": 2,
  "jsonrpc": "2.0",
  "result": {
    "tools": [
      {
        "name": "github_get_repo_info",
        "description": "Get basic repository information",
        "inputSchema": {
          "type": "object",
          "properties": {
            "owner": {"type": "string", "description": "Repository owner"},
            "repo": {"type": "string", "description": "Repository name"}
          },
          "required": ["owner", "repo"]
        }
      },
      {
        "name": "github_summarize_repo",
        "description": "Get comprehensive repository summary",
        "inputSchema": {
          "type": "object",
          "properties": {
            "owner": {"type": "string"},
            "repo": {"type": "string"}
          },
          "required": ["owner", "repo"]
        }
      }
    ]
  }
}
```

**4. tools/call(真实调用)**

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:9876/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"github_get_repo_info\",\"arguments\":{\"owner\":\"bytedance\",\"repo\":\"deer-flow\"}}}"
```

响应:

```json
{
  "id": 3,
  "jsonrpc": "2.0",
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"name\":\"deer-flow\",\"full_name\":\"bytedance/deer-flow\",\"stargazers_count\":12345,...}"
      }
    ]
  }
}
```

**5. shutdown(优雅停止)**

```powershell
curl.exe --noproxy "*" -X POST http://127.0.0.1:9876/mcp ^
  -H "Content-Type: application/json" ^
  -d "{\"jsonrpc\":\"2.0\",\"id\":99,\"method\":\"shutdown\"}"
```

#### LLM 对话调用示例

llama.cpp 启动后,LLM 在对话中会自动生成如下 tool_call:

```json
{
  "tool": "McpServer",
  "parameters": {
    "name": "github_summarize_repo",
    "arguments": {"owner": "bytedance", "repo": "deer-flow"}
  }
}
```

用户问:`请对 bytedance/deer-flow 进行深度研究`

LLM 自动按以下顺序调用:

```
1. McpServer(name="github_summarize_repo",      arguments={owner, repo})
2. McpServer(name="github_get_readme",          arguments={owner, repo})
3. McpServer(name="github_get_tree",            arguments={owner, repo, max_depth=3})
4. McpServer(name="github_get_commits",         arguments={owner, repo, limit=50})
5. McpServer(name="github_get_issues",          arguments={owner, repo, state="all", limit=30})
6. McpServer(name="github_get_pull_requests",   arguments={owner, repo, state="all", limit=30})
7. McpServer(name="github_get_releases",        arguments={owner, repo, limit=10})
```

### 角色 2:llama.cpp 作为 MCP Server

llama.cpp server 自身也可作为 MCP server 暴露给其他 client(如 Claude Desktop),通过 `--mcp-server` 开启。

```powershell
llama-server.exe ^
  -m Qwen2.5-7B-Instruct.Q4_K_M.gguf ^
  --port 8080 ^
  --mcp-server
```

开启后 llama.cpp 在 `/mcp` 路径暴露 `llama_chat` 工具,其他 client 可通过 MCP 协议调用 LLM 对话能力。

**与 github-research-mcp 串联**:

```
Claude Desktop
    │ (MCP over stdio)
    ▼
llama.cpp server (--mcp-server, port 8080)
    │ (HTTP, --mcp)
    ▼
github-research-mcp (--port 9876)
    │ (WebView2)
    ▼
api.github.com
```

Claude Desktop 配置(`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "llama-llm": {
      "command": "llama-server.exe",
      "args": ["-m", "Qwen2.5-7B.gguf", "--mcp-server"],
      "env": {}
    },
    "github-research": {
      "command": "D:\\DeerFlow\\DeerFlow++\\build\\Release\\github-research-mcp.exe",
      "args": [],
      "env": {
        "GITHUB_TOKEN": "ghp_xxx"
      }
    }
  }
}
```

### 协议兼容性

| 协议项 | 支持情况 | 说明 |
|---|---|---|
| JSON-RPC 2.0 | ✅ | 标准 request/response/notification |
| MCP 版本 | 2024-11-05 | `initialize` 协议握手版本 |
| 传输方式 | stdio + HTTP | HTTP 路径 `/mcp`,Content-Type `application/json` |
| 批量请求 | ✅ | JSON 数组形式的批量 JSON-RPC |
| CORS | ✅ | 响应头 `Access-Control-Allow-Origin: *` |
| OPTIONS 预检 | ✅ | 自动返回 200 |
| Streamable HTTP | 部分 | 当前为同步响应,未实现 SSE 长连接 |
| `tools/list` | ✅ | 10 个 `github_*` 工具 |
| `tools/call` | ✅ | 支持 `isError` 字段标记失败 |
| `ping` | ✅ | 心跳保活 |
| `shutdown` | ✅ | 触发 server 优雅停止 |

### 语义调用对照表

| 用户意图 | 调用工具 | 关键参数 |
|---|---|---|
| 看仓库基本信息 | `github_get_repo_info` | `owner`, `repo` |
| 看综合摘要 | `github_summarize_repo` | `owner`, `repo` |
| 读 README | `github_get_readme` | `owner`, `repo` |
| 看目录结构 | `github_get_tree` | `owner`, `repo`, `max_depth=3`, `recursive=true` |
| 看语言分布 | `github_get_languages` | `owner`, `repo` |
| 看贡献者 | `github_get_contributors` | `owner`, `repo`, `limit=30` |
| 看近期提交 | `github_get_commits` | `owner`, `repo`, `limit=50`, `branch`(可选) |
| 看全部分支 | `github_get_branches` | `owner`, `repo`, `limit=100` |
| 搜索仓库 | `github_search_repositories` | `q`, `sort`(可选), `order`, `limit`, `page` |
| 搜索作者 | `github_search_users` | `q`, `sort`(可选), `order`, `limit`, `page` |
| 看 Issues | `github_get_issues` | `owner`, `repo`, `state="all"`, `limit=30` |
| 看 PR | `github_get_pull_requests` | `owner`, `repo`, `state="all"`, `limit=30` |
| 看发布历史 | `github_get_releases` | `owner`, `repo`, `limit=10` |

### 故障排查

**`Connection failed during initialize: capabilities.tools expected object`**

已修复(2026-08-03)。原因是 `capabilities.tools` 曾返回 `null`,现返回 `{}`。请使用最新构建的 exe。

**`HTTP 426 Upgrade Required`**

llama.cpp 早期版本要求 SSE 长连接。本服务当前为同步 HTTP,请使用 `b4000+` 版本的 llama.cpp(支持 Streamable HTTP 同步模式)。

**`WebView2 initialization timeout`**

Edge Runtime 缺失或被沙箱阻止。检查:
1. `reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}" /v pv` 应返回版本号
2. 在非沙箱环境(真实 Windows 终端)运行
3. 设置 `WEBVIEW2_USER_DATA_DIR` 环境变量到可写目录

**`fetch error: Failed to fetch`**

CORS 或网络问题。GitHub API 返回 `Access-Control-Allow-Origin: *`,正常情况下不会触发 CORS。检查系统代理设置。

## 使用

配置完成后,在客户端中对任意 GitHub 仓库发起深度研究请求,例如:

```
请对 bytedance/deer-flow 进行深度研究,生成完整报告
```

客户端会按 `system_prompt.md` 中的 4 轮工作流自动调用工具,最终生成 `research_bytedance_deer-flow_{YYYYMMDD}.md` 报告。

## 测试

```powershell
cd D:\DeerFlow\DeerFlow++
cmake --build build --config Release --target test_smoke
.\build\Release\test_smoke.exe
```

测试覆盖:
- 字符串工具(url_encode / to_lower / iequals)
- WebView2 初始化
- 真实 GitHub API 调用(summarize_repo)
- 404 错误处理

## 工具列表

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
| `github_search_repositories` | 按项目名 / 语言 / topic / stars 搜索仓库 |
| `github_search_users` | 按作者名 / 组织 / 地区 / 粉丝数搜索用户 |

### 搜索查询语法示例

`github_search_repositories` 与 `github_search_users` 的 `q` 参数支持 GitHub Search 语法,可叠加多个限定符:

**仓库搜索**(`github_search_repositories`):

| 场景 | `q` 示例 | 说明 |
|---|---|---|
| 按项目名 | `cxvision` | 模糊匹配 name / description / readme |
| 按语言 | `opencv language:C++` | 限定主语言 |
| 按 topic | `cv topic:computer-vision` | 按 GitHub Topics 分类 |
| 按热度 | `vision stars:>1000` | stars 下限 |
| 按活跃 | `cv pushed:>2025-01-01` | 排除僵尸项目 |
| 按作者 | `cv user:opencv` | 限定 owner |
| 按许可证 | `cv license:MIT` | 按 SPDX 许可证 |
| 综合查询 | `vision language:C++ stars:>500 pushed:>2025-01-01 sort:stars` | 多限定符叠加 |

**作者搜索**(`github_search_users`):

| 场景 | `q` 示例 | 说明 |
|---|---|---|
| 按名字 | `cxvisionai` | login / fullname 匹配 |
| 找组织 | `cv type:org` | 只搜 Organization |
| 找个人 | `cv type:user` | 只搜 User |
| 按粉丝 | `cv followers:>100` | 粉丝数下限 |
| 按地区 | `cv location:China` | 按 profile location |
| 按语言 | `cv language:C++` | 按 profile 语言 |
| 按产出 | `cv repos:>50` | 公开仓库数下限 |

**排序方式**:

| `sort` 值 | 仓库 | 用户 |
|---|---|---|
| `stars` | ⭐ star 数 | — |
| `forks` | fork 数 | — |
| `updated` | 最近更新 | — |
| `followers` | — | ⭐ 粉丝数 |
| `repositories` | — | 公开仓库数 |
| `joined` | — | 注册时间 |
| 省略 | best-match | best-match |

`order` 取值:`desc`(默认,降序)/ `asc`(升序)。

## 错误处理

所有错误返回 `isError=true`,content 为 JSON 字符串:

```json
{"error":"repository not found","status_code":404,"url":"https://api.github.com/repos/..."}
```

限流错误附带 `reset_at`:

```json
{"error":"rate limit exceeded","status_code":429,"reset_at":"1785724800"}
```

## 与 DeerFlow 原版的差异

| 维度 | DeerFlow 原版 | 本地 MCP 版 |
|---|---|---|
| 实现语言 | Python skill + agent runtime | C++17 可执行文件 + LLM 客户端 |
| HTTP 后端 | Python requests | WebView2(Chromium 内核) |
| 浏览器指纹 | 无 | 完整(与 Edge 一致) |
| 反爬能力 | 弱 | 强(真实浏览器) |
| 编排主体 | agent runtime | 本地 LLM 客户端 |
| 记忆 | DeerMem 持久化 | 无(需另接 mem0 MCP) |
| 平台 | 跨平台 | Windows 10/11 专属 |

## 许可

随 DeerFlow 仓库分发。
