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

# ── 共享核心库（注册表读写逻辑，供 Shell 扩展与 UI 复用）──────
add_library(GpuSelectCore STATIC
    src/RegistryUtil.cpp
)
target_include_directories(GpuSelectCore PUBLIC src)
target_link_libraries(GpuSelectCore PUBLIC advapi32)

# ── Shell 扩展 DLL ────────────────────────────────────────
add_library(GpuSelect SHARED
    src/dllmain.cpp
    src/GpuSelectCommand.cpp
    src/SubCommand.cpp
    src/GpuSelect.def
)

target_include_directories(GpuSelect PRIVATE src)

# Shell API（IShellItemArray 等）
target_link_libraries(GpuSelect PRIVATE
    GpuSelectCore
    shell32
    shlwapi
    ole32
    user32
)

# ── 图形界面程序（prd2：ListView + 刷新按钮 + 亲和性菜单）────
add_executable(GpuSelectUI WIN32
    src/ui/GpuSelectUI.cpp
)

target_include_directories(GpuSelectUI PRIVATE src)
target_link_libraries(GpuSelectUI PRIVATE
    GpuSelectCore
    comctl32
    user32
    gdi32
    shlwapi
    shell32
)

# ── 安装（可选） ──────────────────────────────────────────
install(TARGETS GpuSelectCore GpuSelect GpuSelectUI
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
