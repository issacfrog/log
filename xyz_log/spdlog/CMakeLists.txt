# Copyright(c) 2019 spdlog authors Distributed under the MIT License (http://opensource.org/licenses/MIT)

# 设置 CMake 最低版本要求：3.10 到 3.21 之间
cmake_minimum_required(VERSION 3.10...3.21)

# ---------------------------------------------------------------------------------------
# 启动 spdlog 项目
# ---------------------------------------------------------------------------------------
# 版本获取
include(cmake/utils.cmake)
include(cmake/ide.cmake)

spdlog_extract_version()

project(spdlog VERSION ${SPDLOG_VERSION} LANGUAGES CXX)
message(STATUS "Build spdlog: ${SPDLOG_VERSION}")

# 包含 GNU 安装目录标准（定义 CMAKE_INSTALL_INCLUDEDIR, CMAKE_INSTALL_LIBDIR 等）
include(GNUInstallDirs)

# ---------------------------------------------------------------------------------------
# 设置默认构建类型为 Release
# ---------------------------------------------------------------------------------------
# 如果用户没有指定构建类型，且不是多配置生成器（如 Visual Studio），则默认使用 Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

# ---------------------------------------------------------------------------------------
# 编译器配置
# ---------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)          # 注默认不再使用c++20中的std::format
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SPDLOG_BUILD_TEST "Build all artifacts" OFF) # 构建测试
option(SPDLOG_BUILD_SHARED "Build shared library" OFF) # 构建共享库选项（默认构建静态库）
option(SPDLOG_BUILD_PIC "Build position independent code (-fPIC)" OFF) # 构建位置无关代码选项（用于生成共享库，-fPIC 标志）
option(SPDLOG_ENABLE_PCH "Build static or shared library using precompiled header to speed up compilation time" OFF) # 预编译头选项（可以加速编译时间）

# Debug 构建的后缀（Debug 模式下库文件名会添加 "d" 后缀，如 libspdlogd.a）
set(SPDLOG_DEBUG_POSTFIX "d" CACHE STRING "Filename postfix for libraries in debug builds")

# 内存检查工具选项（用于检测内存错误和线程问题）
option(SPDLOG_SANITIZE_ADDRESS "Enable address sanitizer in tests" OFF)
option(SPDLOG_SANITIZE_THREAD "Enable thread sanitizer in tests" OFF)
# 地址检查器和线程检查器不能同时启用（互斥）
if(SPDLOG_SANITIZE_ADDRESS AND SPDLOG_SANITIZE_THREAD)
    message(FATAL_ERROR "SPDLOG_SANITIZE_ADDRESS and SPDLOG_SANITIZE_THREAD are mutually exclusive")
endif()

# 编译器警告选项
option(SPDLOG_BUILD_WARNINGS "Enable compiler warnings" OFF)

# 安装选项
# 将头文件作为系统头文件包含（跳过 clang-tidy 检查）
option(SPDLOG_SYSTEM_INCLUDES "Include as system headers (skip for clang-tidy)." OFF)
option(SPDLOG_NO_EXCEPTIONS "Compile with -fno-exceptions. Call abort() on any spdlog exceptions" OFF) # 禁用异常处理（使用 -fno-exceptions，异常时调用 abort()）

option(SPDLOG_CLOCK_COARSE "Use CLOCK_REALTIME_COARSE instead of the regular clock," OFF) # Linux 平台特定的粗粒度时钟选项（使用 CLOCK_REALTIME_COARSE 替代常规时钟，性能更好）
option(SPDLOG_PREVENT_CHILD_FD "Prevent from child processes to inherit log file descriptors" OFF) # 防止子进程继承日志文件描述符（避免文件描述符泄漏）
option(SPDLOG_NO_THREAD_ID "prevent spdlog from querying the thread id on each log call if thread id is not needed" OFF) # 如果不需要线程 ID，可以禁用查询以提升性能
option(SPDLOG_NO_TLS "prevent spdlog from using thread local storage" OFF) # 禁用线程本地存储（TLS）的使用

option(
    SPDLOG_NO_ATOMIC_LEVELS
    "prevent spdlog from using of std::atomic log levels (use only if your code never modifies log levels concurrently"
    OFF) # 禁用原子日志级别（仅在代码不会并发修改日志级别时使用，可提升性能）

option(SPDLOG_DISABLE_DEFAULT_LOGGER "Disable default logger creation" OFF) # 禁用默认 logger 的创建
option(SPDLOG_FWRITE_UNLOCKED "Use the unlocked variant of fwrite. Leave this on unless your libc doesn't have it" ON) # 使用无锁版本的 fwrite（性能更好，除非你的 libc 不支持）

# 如果启用了位置无关代码选项,则设置全局标志,静态时需要考虑
if(SPDLOG_BUILD_PIC AND NOT SPDLOG_BUILD_SHARED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# 查找线程库（pthread 等），spdlog 需要它
find_package(Threads REQUIRED)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
# ---------------------------------------------------------------------------------------
# 静态/共享库构建
# ---------------------------------------------------------------------------------------
set(SPDLOG_SRCS 
    src/spdlog.cpp 
    src/stdout_sinks.cpp 
    src/color_sinks.cpp 
    src/file_sinks.cpp 
    src/async.cpp 
    src/cfg.cpp)

list(APPEND SPDLOG_SRCS src/bundled_fmtlib_format.cpp) # 默认使用内部fmt库，删除外部的fmt选项

# 静态库or动态库
if(SPDLOG_BUILD_SHARED) 
    add_library(spdlog SHARED ${SPDLOG_SRCS} ${SPDLOG_ALL_HEADERS})
    target_compile_definitions(spdlog PUBLIC SPDLOG_SHARED_LIB)
    target_compile_definitions(spdlog PRIVATE FMT_LIB_EXPORT PUBLIC FMT_SHARED)
else()
    add_library(spdlog STATIC ${SPDLOG_SRCS} ${SPDLOG_ALL_HEADERS})
endif()

add_library(spdlog::spdlog ALIAS spdlog)
set(SPDLOG_INCLUDES_LEVEL "")
if(SPDLOG_SYSTEM_INCLUDES)
    set(SPDLOG_INCLUDES_LEVEL "SYSTEM")
endif()

target_compile_definitions(spdlog PUBLIC SPDLOG_COMPILED_LIB)
target_include_directories(spdlog ${SPDLOG_INCLUDES_LEVEL} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                                                                  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_link_libraries(spdlog PUBLIC Threads::Threads)
spdlog_enable_warnings(spdlog) # 启用编译器警告

# 设置库的版本属性（用于共享库的版本控制）
set_target_properties(spdlog PROPERTIES VERSION ${SPDLOG_VERSION} SOVERSION
                                                                  ${SPDLOG_VERSION_MAJOR}.${SPDLOG_VERSION_MINOR})
set_target_properties(spdlog PROPERTIES DEBUG_POSTFIX ${SPDLOG_DEBUG_POSTFIX})

# 如果支持预编译头且启用了该选项，则配置预编译头
if(COMMAND target_precompile_headers AND SPDLOG_ENABLE_PCH)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/pch.h.in ${PROJECT_BINARY_DIR}/spdlog_pch.h @ONLY)
    target_precompile_headers(spdlog PRIVATE ${PROJECT_BINARY_DIR}/spdlog_pch.h)
endif()

# 内存检查工具支持（用于测试）
if(SPDLOG_SANITIZE_ADDRESS)
    spdlog_enable_addr_sanitizer(spdlog)
elseif(SPDLOG_SANITIZE_THREAD)
    spdlog_enable_thread_sanitizer(spdlog)
endif()

# ---------------------------------------------------------------------------------------
# Header-only 版本（仅头文件版本）
# ---------------------------------------------------------------------------------------
# 创建接口库目标（不编译任何源文件，只提供头文件和链接选项）
add_library(spdlog_header_only INTERFACE)
add_library(spdlog::spdlog_header_only ALIAS spdlog_header_only)

# 设置头文件包含目录（INTERFACE 表示仅传递给依赖此库的目标）
target_include_directories(
    spdlog_header_only ${SPDLOG_INCLUDES_LEVEL} INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
                                                          "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
# 链接线程库（INTERFACE 表示仅传递给依赖此库的目标）
target_link_libraries(spdlog_header_only INTERFACE Threads::Threads)

# ---------------------------------------------------------------------------------------
# 检查 fwrite_unlocked/_fwrite_nolock 是否可用 无锁版本性能更好
# ---------------------------------------------------------------------------------------
if(SPDLOG_FWRITE_UNLOCKED)
    include(CheckSymbolExists) 
    check_symbol_exists(fwrite_unlocked "stdio.h" HAVE_FWRITE_UNLOCKED) 
    # 如果符号存在，则定义宏以使用无锁版本
    if(HAVE_FWRITE_UNLOCKED) # 如果符号存在，则定义宏以使用无锁版本
        target_compile_definitions(spdlog PRIVATE SPDLOG_FWRITE_UNLOCKED)
        target_compile_definitions(spdlog_header_only INTERFACE SPDLOG_FWRITE_UNLOCKED)
    endif()
endif()

# ---------------------------------------------------------------------------------------
# 根据可调选项设置编译定义
# ---------------------------------------------------------------------------------------
# 遍历所有可调选项，如果启用则添加到编译定义中
foreach(
    SPDLOG_OPTION
    SPDLOG_NO_EXCEPTIONS
    SPDLOG_CLOCK_COARSE
    SPDLOG_PREVENT_CHILD_FD
    SPDLOG_NO_THREAD_ID
    SPDLOG_NO_TLS
    SPDLOG_NO_ATOMIC_LEVELS
    SPDLOG_DISABLE_DEFAULT_LOGGER
    SPDLOG_USE_STD_FORMAT)
    if(${SPDLOG_OPTION})
        target_compile_definitions(spdlog PUBLIC ${SPDLOG_OPTION})
        target_compile_definitions(spdlog_header_only INTERFACE ${SPDLOG_OPTION})
    endif()
endforeach()

# ---------------------------------------------------------------------------------------
# 如果禁用了异常，也要在打包的 fmt 中禁用异常
# ---------------------------------------------------------------------------------------
if(SPDLOG_NO_EXCEPTIONS)
    target_compile_definitions(spdlog PUBLIC FMT_USE_EXCEPTIONS=0) # 注，默认使用内部fmt库
    target_compile_options(spdlog PRIVATE -fno-exceptions)
endif()

# ---------------------------------------------------------------------------------------
# 构建测试
# ---------------------------------------------------------------------------------------
if(SPDLOG_BUILD_TEST)
    message(STATUS "Generating tests")
    enable_testing()  # 启用 CTest 测试框架
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------------------
# 安装配置
# ---------------------------------------------------------------------------------------
set(SPDLOG_INSTALL ON)
if(SPDLOG_INSTALL)
    message(STATUS "Generating install")
    set(project_config_in "${CMAKE_CURRENT_LIST_DIR}/cmake/spdlogConfig.cmake.in")
    set(project_config_out "${CMAKE_CURRENT_BINARY_DIR}/spdlogConfig.cmake")
    set(config_targets_file "spdlogConfigTargets.cmake")
    set(version_config_file "${CMAKE_CURRENT_BINARY_DIR}/spdlogConfigVersion.cmake")
    set(export_dest_dir "${CMAKE_INSTALL_LIBDIR}/cmake/spdlog")
    set(pkgconfig_install_dir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    set(pkg_config "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc")

    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" PATTERN "fmt/bundled" EXCLUDE)
    install(
        TARGETS spdlog spdlog_header_only
        EXPORT spdlog
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    if(NOT SPDLOG_USE_STD_FORMAT AND NOT SPDLOG_FMT_EXTERNAL AND NOT SPDLOG_FMT_EXTERNAL_HO)
        install(DIRECTORY include/${PROJECT_NAME}/fmt/bundled/
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/fmt/bundled/")
    endif()

    # ---------------------------------------------------------------------------------------
    # 安装 pkg-config 文件
    # ---------------------------------------------------------------------------------------
    # 设置 pkg-config 文件中的包含目录路径（处理绝对路径和相对路径）
    if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
        set(PKG_CONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
        set(PKG_CONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()
    # 设置 pkg-config 文件中的库目录路径
    if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
        set(PKG_CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    else()
        set(PKG_CONFIG_LIBDIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    endif()
    # 获取编译定义并格式化为 pkg-config 格式（-D 标志）
    get_target_property(PKG_CONFIG_DEFINES spdlog INTERFACE_COMPILE_DEFINITIONS)
    string(REPLACE ";" " -D" PKG_CONFIG_DEFINES "${PKG_CONFIG_DEFINES}")
    string(CONCAT PKG_CONFIG_DEFINES "-D" "${PKG_CONFIG_DEFINES}")
    # 从模板生成 pkg-config 文件并安装
    configure_file("cmake/${PROJECT_NAME}.pc.in" "${pkg_config}" @ONLY)
    install(FILES "${pkg_config}" DESTINATION "${pkgconfig_install_dir}")

    # ---------------------------------------------------------------------------------------
    # 安装 CMake 配置文件
    # ---------------------------------------------------------------------------------------
    # 导出目标到构建目录（用于开发时使用）
    export(TARGETS spdlog spdlog_header_only NAMESPACE spdlog::
           FILE "${CMAKE_CURRENT_BINARY_DIR}/${config_targets_file}")
    # 安装导出文件到安装目录（用于安装后使用）
    install(EXPORT spdlog DESTINATION ${export_dest_dir} NAMESPACE spdlog:: FILE ${config_targets_file})

    # 包含 CMake 包配置辅助模块
    include(CMakePackageConfigHelpers)
    # 从模板生成包配置文件（spdlogConfig.cmake）
    configure_package_config_file("${project_config_in}" "${project_config_out}" INSTALL_DESTINATION ${export_dest_dir})

    # 生成版本配置文件（spdlogConfigVersion.cmake），用于版本兼容性检查
    # SameMajorVersion: 只允许相同主版本号
    write_basic_package_version_file("${version_config_file}" COMPATIBILITY SameMajorVersion)
    # 安装配置文件
    install(FILES "${project_config_out}" "${version_config_file}" DESTINATION "${export_dest_dir}")
    include(cmake/spdlogCPack.cmake)
endif()