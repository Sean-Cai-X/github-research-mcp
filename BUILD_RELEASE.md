# 发布操作说明(Build & Publish Release Binary)

本项目通过 GitHub Actions 实现「代码进,程序出」:推送 `v*` 标签 → 云端自动编译 → 打包 zip → 挂载到 Release → 用户直接下载 exe。

## 流水线文件

- 配置:`.github/workflows/release-build.yml`
- 触发条件:`push.tags: v*`(仅推送 `v` 开头的 tag 才触发)
- 运行环境:`windows-latest`(GitHub 托管的 Windows Runner)
- 鉴权:内置 `GITHUB_TOKEN`(`permissions: contents: write`),无需手动创建 PAT

## 云端依赖补齐(全部走网络源,本地 DLL/EXE/图像不上传到库)

本地 `third_party/WebView2/lib/`、`third_party/WebView2/redist/`、`third_party/json/` 被 `.gitignore` 忽略,云端检出仓库后缺失。流水线在编译前自动下载补齐:

| 依赖 | 来源 | 落地位置 |
|---|---|---|
| WebView2 SDK | NuGet `Microsoft.Web.WebView2` 1.0.2792.45 | `third_party/WebView2/{include,lib/x64,redist/x64}` |
| nlohmann/json | GitHub Release `nlohmann/json` v3.11.3 单头 | `third_party/json/include/nlohmann/json.hpp` |
| SQLite | 仓库内置 `third_party/sqlite/sqlite3.c` | 随源码提交 |

## 静态 CRT(关键,避免纯净机器缺 msvcrt.dll)

- CMakeLists 已在 `research-mcp` / `test_smoke` 两个 target 上显式设置:
  - `MSVC_RUNTIME_LIBRARY "MultiThreaded"`
  - 编译选项 `/MT /W3 /EHsc /utf-8`
- 流水线 `cmake` 命令额外传 `-DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded"` 作为全局默认双保险
- 结果:exe 静态链接 CRT,纯净 Windows 10/11 x64 机器无需安装 VC++ 运行库

## 打包内容

zip `research-mcp-windows-x64.zip` 内含:

- `research-mcp.exe`(主程序,静态 CRT)
- `WebView2Loader.dll`(WebView2 运行时必需,由 CMake POST_BUILD 复制到 exe 同目录)
- `mcp_config_example.json`(若存在)
- `.env.example`(若存在)
- `README.md`(若存在)

## 标准发布流程

### 1. 打语义化版本标签

```bash
git tag -a v0.1.0 -m "release v0.1.0 research-mcp"
```

### 2. 推送 tag 触发云端构建

```bash
git push origin v0.1.0
```

推送后前往仓库 **Actions** 页面查看构建进度,构建成功后自动在 **Releases** 页面生成 `research-mcp-windows-x64.zip`。

### 3. 本地代理(如需,仅本地 git push 时用)

```powershell
$Proxy = "http://127.0.0.1:7897"
$env:HTTPS_PROXY = $Proxy
$env:HTTP_PROXY  = $Proxy
$env:ALL_PROXY   = $Proxy
```

> 云端 GitHub Actions Runner 自有网络,不需要此代理。此代理仅用于本地 `git push` 走代理出口。

## 构建失败清理错误 tag

```bash
# 删除本地 tag
git tag -d v0.1.0
# 删除远端 tag
git push origin --delete v0.1.0
```

## 发布版本号建议(语义化版本)

| 版本 | 适用场景 |
|---|---|
| `v0.x.x` | 早期内测,功能未稳定 |
| `v1.0.0` | 首个正式发布 |
| `v1.x.0` | 新功能发布 |
| `v1.x.y` | Bug 修复补丁 |

## 验收清单(每次发布后人工核对)

- [ ] Actions 构建状态为绿色(✅)
- [ ] Releases 页面出现对应 tag 的 Release
- [ ] Release 附件含 `research-mcp-windows-x64.zip`
- [ ] 下载 zip 解压后含 `research-mcp.exe` 和 `WebView2Loader.dll`
- [ ] 在一台**纯净** Windows 机器(未装 VS / VC++ 运行库)上运行 exe 能正常启动
- [ ] 启动后 `tools/list` 返回 65 个工具

## 本地复现云端构建(可选)

```powershell
cd D:\DeerFlow\DeerFlow++
# 确保 third_party/WebView2 和 third_party/json 已就位(本地开发环境)
mkdir build-release
cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded" ..
cmake --build . --config Release
# 产物:build-release\Release\research-mcp.exe + WebView2Loader.dll
```
