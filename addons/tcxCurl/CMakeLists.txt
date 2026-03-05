# =============================================================================
# tcxCurl - HTTPS Client Addon (libcurl)
# =============================================================================

cmake_minimum_required(VERSION 3.20)
message(STATUS "[tcxCurl] Loading addon...")

add_library(tcxCurl INTERFACE)
target_include_directories(tcxCurl INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(EMSCRIPTEN)
    # WASM: use Emscripten Fetch API
    target_compile_definitions(tcxCurl INTERFACE TCX_HTTP_EMSCRIPTEN=1)
    target_link_options(tcxCurl INTERFACE -sFETCH=1)
else()
    # Native: use libcurl
    find_package(CURL QUIET)
    if(NOT CURL_FOUND)
        message(FATAL_ERROR
            "[tcxCurl] libcurl not found!\n"
            "\n"
            "  tcxCurl requires libcurl for HTTPS support.\n"
            "\n"
            "  macOS:    brew install curl  (or use system curl)\n"
            "  Linux:    sudo apt install libcurl4-openssl-dev\n"
            "  Windows:  vcpkg install curl:x64-windows\n"
            "            then configure with:\n"
            "            cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake ..\n"
        )
    endif()
    target_compile_definitions(tcxCurl INTERFACE TCX_HTTP_CURL=1)
    target_link_libraries(tcxCurl INTERFACE CURL::libcurl)
endif()

message(STATUS "[tcxCurl] Addon loaded")
