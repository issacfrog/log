cmake_minimum_required(VERSION 3.8)
project(my_log)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

# 配置选项：选择日志后端
option(USE_GLOG "Use glog as logging backend (default: use spdlog)" OFF)
option(BUILD_TESTING "Build tests" ON)

# 选择使用glog还是spdlog作为日志后端
if(NOT USE_GLOG)
  set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as shared library" FORCE)
  set(SPDLOG_BUILD_TEST OFF CACHE BOOL "Build spdlog tests" FORCE)
  # 必须启用 PIC，因为我们要将 spdlog 静态库链接到共享库 my_log
  set(SPDLOG_BUILD_PIC ON CACHE BOOL "Build position independent code" FORCE)
  add_subdirectory(spdlog)
  
  message(STATUS "Using spdlog as logging backend")
  message(STATUS "spdlog include dir: ${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include")
else()
  find_package(glog REQUIRED)
  message(STATUS "Using glog as logging backend")
endif()

# 包含目录
include_directories(include)

# 创建库
# 在 spdlog 模式下，实现都在头文件中，源文件只包含 glog 的实现
# 在 glog 模式下，源文件包含 glog 的实现
add_library(${PROJECT_NAME} SHARED
  src/logging.cpp
)

# 设置 C++ 标准（需要 C++17 以支持 std::filesystem）
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

# 某些编译器需要链接 filesystem 库
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
  target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.0")
  target_link_libraries(${PROJECT_NAME} PRIVATE c++fs)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# 如果使用 spdlog，确保包含 spdlog 的头文件路径
if(NOT USE_GLOG)
  # spdlog 的 include 目录会通过 target_link_libraries 自动传递
  # 但我们也显式添加以确保编译时能找到头文件
  target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include>
  )
  # 同时添加到 PUBLIC，以便使用此库的其他目标也能找到 spdlog
  target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include>
  )
  
  # 调试信息
  if(TARGET spdlog::spdlog)
    message(STATUS "spdlog::spdlog target found")
    get_target_property(SPDLOG_INCLUDE_DIRS spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "spdlog include dirs: ${SPDLOG_INCLUDE_DIRS}")
  else()
    message(WARNING "spdlog::spdlog target not found!")
  endif()
endif()

# 链接依赖
target_link_libraries(${PROJECT_NAME} PUBLIC
  rclcpp::rclcpp
)

if(USE_GLOG)
  target_link_libraries(${PROJECT_NAME} PUBLIC
    glog::glog
  )
  target_compile_definitions(${PROJECT_NAME} PUBLIC USE_GLOG)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GLOG)
  message(STATUS "Linked with glog")
else()
  if(TARGET spdlog::spdlog)
    target_link_libraries(${PROJECT_NAME} PUBLIC
      spdlog::spdlog
    )
    # spdlog 使用内部 fmt 库（bundled_fmtlib_format.cpp），不需要单独链接 fmt::fmt
    # 注意：spdlog 模式下，实现都在头文件中（header-only），不需要编译源文件中的 spdlog 部分
    message(STATUS "Linked with spdlog::spdlog (includes bundled fmt)")
  else()
    message(FATAL_ERROR "spdlog::spdlog target not found! Cannot link.")
  endif()
endif()

# 安装
install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# 测试
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()


  # 添加简单测试可执行文件（不需要 ROS2）
  add_executable(simple_test test/simple_test.cpp)
  target_link_libraries(simple_test ${PROJECT_NAME})
  # 简单测试不需要 rclcpp，因为它不初始化 ROS2
  
  # 安装 simple_test 到 bin 目录
  install(TARGETS simple_test
    RUNTIME DESTINATION lib/${PROJECT_NAME}
  )
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(export_${PROJECT_NAME})

ament_package()
