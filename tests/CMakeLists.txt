# test_smoke 直接编译所需源文件,不链接 github-research-mcp(EXECUTABLE 不可被链接)
add_executable(test_smoke
    test_smoke.cpp
    ${CMAKE_SOURCE_DIR}/src/github_client.cpp
    ${CMAKE_SOURCE_DIR}/src/webview_client.cpp
    ${CMAKE_SOURCE_DIR}/src/winhttp_client.cpp
    ${CMAKE_SOURCE_DIR}/src/formatters.cpp
    ${CMAKE_SOURCE_DIR}/src/string_utils.cpp
)
target_include_directories(test_smoke PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${JSON_INCLUDE_DIR}
    ${WEBVIEW2_INCLUDE_DIR}
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(test_smoke PRIVATE nlohmann_json::nlohmann_json)
endif()
if(TARGET WebView2::WebView2)
    target_link_libraries(test_smoke PRIVATE WebView2::WebView2)
else()
    target_link_libraries(test_smoke PRIVATE ${WEBVIEW2_LIB_DIR}/WebView2Loader.dll.lib)
endif()

target_link_libraries(test_smoke PRIVATE user32 ole32 oleaut32 runtimeobject)

if(MSVC)
    set_property(TARGET test_smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(test_smoke PRIVATE /MT /W3 /EHsc /utf-8)
    target_compile_definitions(test_smoke PRIVATE NOMINMAX)
endif()

add_test(NAME test_smoke COMMAND test_smoke)
