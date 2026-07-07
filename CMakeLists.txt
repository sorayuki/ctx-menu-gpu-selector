cmake_minimum_required(VERSION 3.20)
project(GpuSelect VERSION 1.0.0 LANGUAGES CXX)

# ── 编译选项 ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 静态链接 CRT（/MT 或 /MTd）
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Unicode 字符集
add_compile_definitions(UNICODE _UNICODE)

# MSVC 按 UTF-8 解析源码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 关闭 ATL 所有权警告
add_compile_options(/W3 /wd4467)

# ── 源文件 ────────────────────────────────────────────────
add_library(GpuSelect SHARED
    src/dllmain.cpp
    src/GpuSelectCommand.cpp
    src/SubCommand.cpp
    src/RegistryUtil.cpp
    src/GpuSelect.def
)

target_include_directories(GpuSelect PRIVATE src)

# ── 链接 ──────────────────────────────────────────────────
# Shell API（IShellItemArray 等）
target_link_libraries(GpuSelect PRIVATE
    shell32
    shlwapi
    ole32
    advapi32
    user32
)

# ── 安装（可选） ──────────────────────────────────────────
install(TARGETS GpuSelect
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
