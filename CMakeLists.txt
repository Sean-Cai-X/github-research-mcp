cmake_minimum_required(VERSION 3.16)
project(github-research-mcp VERSION 0.1.0 LANGUAGES CXX)

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

add_executable(github-research-mcp
    src/main.cpp
    src/mcp_server.cpp
    src/http_server.cpp
    src/tools.cpp
    src/github_client.cpp
    src/webview_client.cpp
    src/winhttp_client.cpp
    src/formatters.cpp
    src/string_utils.cpp
)

target_include_directories(github-research-mcp PRIVATE
    include
    ${JSON_INCLUDE_DIR}
    ${WEBVIEW2_INCLUDE_DIR}
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(github-research-mcp PRIVATE nlohmann_json::nlohmann_json)
endif()

if(TARGET WebView2::WebView2)
    target_link_libraries(github-research-mcp PRIVATE WebView2::WebView2)
else()
    target_link_libraries(github-research-mcp PRIVATE ${WEBVIEW2_LIB_DIR}/WebView2Loader.dll.lib)
endif()

# Windows 系统库
target_link_libraries(github-research-mcp PRIVATE
    user32      # 窗口 API
    ole32       # COM
    oleaut32    # COM 辅助
    runtimeobject  # Microsoft::WRL::Callback
    ws2_32      # WinSock(HTTP 模式)
)

# 复制 WebView2Loader.dll 到输出目录
set(WEBVIEW2_DLL_PATH ${CMAKE_SOURCE_DIR}/third_party/WebView2/redist/x64/WebView2Loader.dll)
if(EXISTS ${WEBVIEW2_DLL_PATH})
    add_custom_command(TARGET github-research-mcp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${WEBVIEW2_DLL_PATH}
        $<TARGET_FILE_DIR:github-research-mcp>/WebView2Loader.dll)
endif()

# Windows 静态链接 CRT
if(MSVC)
    set_property(TARGET github-research-mcp PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(github-research-mcp PRIVATE /MT /W3 /EHsc /utf-8)
    # 禁用 Windows.h 的 min/max 宏,避免与 std::min/std::max 冲突
    target_compile_definitions(github-research-mcp PRIVATE NOMINMAX)
endif()

# 测试
enable_testing()
add_subdirectory(tests)
