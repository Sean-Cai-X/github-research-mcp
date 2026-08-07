# test_smoke 直接编译所需源文件,不链接 research-mcp(EXECUTABLE 不可被链接)
add_executable(test_smoke
    test_smoke.cpp
    ${CMAKE_SOURCE_DIR}/src/github_client.cpp
    ${CMAKE_SOURCE_DIR}/src/curl_http_client.cpp
    ${CMAKE_SOURCE_DIR}/src/cache_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/source_fusion.cpp
    ${CMAKE_SOURCE_DIR}/src/webview_client.cpp
    ${CMAKE_SOURCE_DIR}/src/formatters.cpp
    ${CMAKE_SOURCE_DIR}/src/string_utils.cpp
    ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c
)
target_include_directories(test_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${JSON_INCLUDE_DIR}
    ${WEBVIEW2_INCLUDE_DIR}
    ${CURL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/sqlite
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(test_smoke PRIVATE nlohmann_json::nlohmann_json)
endif()
if(TARGET WebView2::WebView2)
    target_link_libraries(test_smoke PRIVATE WebView2::WebView2)
else()
    target_link_libraries(test_smoke PRIVATE ${WEBVIEW2_LIB_DIR}/WebView2Loader.dll.lib)
endif()

# libcurl 链接(与 research-mcp 主目标一致)
if(TARGET CURL::libcurl)
    target_link_libraries(test_smoke PRIVATE CURL::libcurl)
elseif(EXISTS ${CURL_LIB_DIR}/libcurl.lib)
    target_link_libraries(test_smoke PRIVATE ${CURL_LIB_DIR}/libcurl.lib)
endif()

target_link_libraries(test_smoke PRIVATE user32 ole32 oleaut32 runtimeobject crypt32 bcrypt)

if(MSVC)
    set_property(TARGET test_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(test_smoke PRIVATE /MT /W3 /EHsc /utf-8)
    target_compile_definitions(test_smoke PRIVATE NOMINMAX)
endif()

# 复制 libcurl-x64.dll 和 CA 证书到测试输出目录(运行时依赖)
if(EXISTS ${CURL_BIN_DIR}/libcurl-x64.dll)
    add_custom_command(TARGET test_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CURL_BIN_DIR}/libcurl-x64.dll
        $<TARGET_FILE_DIR:test_smoke>/libcurl-x64.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CURL_BIN_DIR}/curl-ca-bundle.crt
        $<TARGET_FILE_DIR:test_smoke>/curl-ca-bundle.crt)
endif()

add_test(NAME test_smoke COMMAND test_smoke)
