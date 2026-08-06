cmake_minimum_required(VERSION 3.16)
project(research-mcp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 仅支持 Windows(WebView2 依赖)
if(NOT WIN32)
    message(FATAL_ERROR "This project only supports Windows (WebView2 dependency).")
endif()

# === nlohmann/json: 优先用 vcpkg,否则用单 header(随仓库分发) ===
find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    set(JSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/json/include)
    if(NOT EXISTS ${JSON_INCLUDE_DIR}/nlohmann/json.hpp)
        message(WARNING "nlohmann/json not found via vcpkg, will use single-header at ${JSON_INCLUDE_DIR}")
    endif()
endif()

# === WebView2 SDK: 优先 vcpkg,否则用 third_party/WebView2 ===
find_package(webview2 CONFIG QUIET)
if(NOT webview2_FOUND)
    set(WEBVIEW2_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/WebView2/include)
    set(WEBVIEW2_LIB_DIR ${CMAKE_SOURCE_DIR}/third_party/WebView2/lib/x64)
    if(NOT EXISTS ${WEBVIEW2_INCLUDE_DIR}/WebView2.h)
        message(FATAL_ERROR
            "WebView2 SDK not found. Please download Microsoft.Web.WebView2 NuGet package "
            "and extract to: ${CMAKE_SOURCE_DIR}/third_party/WebView2\n"
            "Expected layout:\n"
            "  third_party/WebView2/include/WebView2.h\n"
            "  third_party/WebView2/include/WebView2Environment.h\n"
            "  third_party/WebView2/lib/x64/WebView2Loader.dll.lib\n"
            "  third_party/WebView2/redist/x64/WebView2Loader.dll")
    endif()
endif()

# === SQLite3: amalgamation 直接编译入 exe (无外部依赖) ===
set(SQLITE_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/sqlite)
if(NOT EXISTS ${SQLITE_INCLUDE_DIR}/sqlite3.h)
    message(FATAL_ERROR "SQLite3 not found at third_party/sqlite/sqlite3.h")
endif()

add_executable(research-mcp
    src/main.cpp
    src/mcp_server.cpp
    src/http_server.cpp
    src/tools.cpp
    src/github_client.cpp
    src/cache_manager.cpp
    src/source_fusion.cpp
    src/webview_client.cpp
    src/webview_session.cpp
    src/webview_helpers.cpp
    src/arxiv_tools.cpp
    src/hackernews_tools.cpp
    src/package_tools.cpp
    src/paperswithcode_tools.cpp
    src/huggingface_tools.cpp
    src/semanticscholar_tools.cpp
    src/stackoverflow_tools.cpp
    src/formatters.cpp
    src/string_utils.cpp
    third_party/sqlite/sqlite3.c
)

target_include_directories(research-mcp PRIVATE
    include
    ${JSON_INCLUDE_DIR}
    ${WEBVIEW2_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(research-mcp PRIVATE nlohmann_json::nlohmann_json)
endif()

if(TARGET WebView2::WebView2)
    target_link_libraries(research-mcp PRIVATE WebView2::WebView2)
else()
    target_link_libraries(research-mcp PRIVATE ${WEBVIEW2_LIB_DIR}/WebView2Loader.dll.lib)
endif()

# Windows 系统库
target_link_libraries(research-mcp PRIVATE
    user32      # 窗口 API
    ole32       # COM
    oleaut32    # COM 辅助
    runtimeobject  # Microsoft::WRL::Callback
    ws2_32      # WinSock(HTTP 模式)
)

# 复制 WebView2Loader.dll 到输出目录
set(WEBVIEW2_DLL_PATH ${CMAKE_SOURCE_DIR}/third_party/WebView2/redist/x64/WebView2Loader.dll)
if(EXISTS ${WEBVIEW2_DLL_PATH})
    add_custom_command(TARGET research-mcp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WEBVIEW2_DLL_PATH}
        $<TARGET_FILE_DIR:research-mcp>/WebView2Loader.dll)
endif()

# Windows 静态链接 CRT
if(MSVC)
    set_property(TARGET research-mcp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(research-mcp PRIVATE /MT /W3 /EHsc /utf-8)
    # 禁用 Windows.h 的 min/max 宏,避免与 std::min/std::max 冲突
    target_compile_definitions(research-mcp PRIVATE NOMINMAX)
endif()

# 测试
enable_testing()
add_subdirectory(tests)
