# my_log
---
对glog和spdlog进行封装以方便使用。
- 量产等场景建议使用spdlog后端以提升log性能
- 研发自测阶段可使用glog，默认开启崩溃log收集方便定位问题

## 特性

- ✅ **glog 兼容 API**：提供与 Google glog 兼容的日志接口（`LOG(INFO)`, `LOG(ERROR)` 等）
- ✅ **双后端支持**：可在编译时选择使用 glog 或 spdlog 后端
- ✅ **Header-only (spdlog)**：spdlog 后端为 header-only 实现，无需链接库
- ✅ **日志轮转**：spdlog按照启动时间+编号方式输出log（实现同GLOG方式的落取需要自定义sink，实测性能下降较多）
- ✅ **配置**：支持日志等级、目录、控制台输出、异步模式等配置
- ✅ **性能**：spdlog支持异步模式，提升日志写入性能，封装后较原生spdlog附加约10%资源消耗（spdlog默认比glog性能提升几倍）

## 支持的 API

### 基本日志宏

```cpp
LOG(TRACE) << "Trace message";
LOG(DEBUG) << "Debug message";
LOG(INFO) << "Information message";
LOG(WARNING) << "Warning message";
LOG(ERROR) << "Error message";
LOG(FATAL) << "Fatal error (will abort)";
```
注由于spdlog不支持LOG_EVERY_N形式，暂时没有进行对应接口实现
### 格式化日志（仅 spdlog 后端）
```cpp
LOG_FMT(INFO, "Message: {}, value: {}", "test", 42);
```

## 编译配置

### 使用 spdlog 后端（默认）

```bash
colcon build --packages-select my_log
```

### 使用 glog 后端

```bash
cd /path/to/ros2_ws
colcon build --packages-select my_log \
  --cmake-args -DUSE_GLOG=ON
```

**注意**：使用 glog 后端需要安装 glog 开发库：
```bash
sudo apt-get install libglog-dev
```

## 使用方法

### 1. 在 CMakeLists.txt 中添加依赖

```cmake
find_package(my_log REQUIRED)
ament_target_dependencies(your_target my_log)
```

### 2. 在代码中使用

```cpp
#include "my_log.hpp"

int main(int argc, char** argv) {
    // 配置日志选项
    my::LoggerOptions opts;
    opts.program_name = "my_app";
    opts.log_level = my::LogLevel::INFO;
    opts.enable_console = true;  // 同时输出到控制台和文件
    opts.async_mode = true;      // 使用异步模式提升性能
    
    // 初始化日志系统
    my::Logger::Init(opts);
    
    // 使用日志
    LOG(INFO) << "Application started";
    LOG(INFO) << "Value: " << 42;
    LOG(ERROR) << "Something went wrong";
    
    // 程序退出前关闭日志系统
    my::Logger::Shutdown();
    return 0;
}
```

### 3. 日志配置选项

```cpp
struct LoggerOptions {
    std::string program_name = "my_app";           // 程序名称，用于日志文件名和目录
    std::string log_dir = "";                       // 日志目录，空字符串使用默认目录 ~/.my_log
    bool enable_console = false;                    // 是否输出到控制台（默认 false）
    bool async_mode = true;                         // 是否使用异步模式（默认 true）
    bool multi_thread = true;                       // 是否使用多线程安全的 sink（默认 true）
    std::size_t async_queue_size = 32768;          // 异步队列大小（默认 32768）
    LogLevel log_level = LogLevel::ERROR;           // 默认日志等级（默认 ERROR）
    std::size_t max_log_size = 10 * 1024 * 1024;   // 单个日志文件最大大小（默认 10MB）
    bool enable_coredump_log = true;                // 是否启用 coredump 日志收集（仅 glog 后端有效，默认 true）
};
```

### 4. 动态设置日志等级

```cpp
// 设置日志等级
my::Logger::SetLogLevel(my::LogLevel::DEBUG);

// 获取当前日志等级
my::LogLevel current_level = my::Logger::GetLogLevel();
```

## 运行测试

```bash
# 构建
colcon build --packages-select my_log

# 运行测试程序
source install/setup.bash
./install/my_log/lib/my_log/simple_test
```

## 项目结构

```
my_log/
├── CMakeLists.txt          # 主构建文件
├── package.xml             # ROS2 包定义
├── include/
│   └── my_log/
│       └── my_log.hpp     # 主头文件（包含所有实现）
├── src/
│   └── logging.cpp         # glog 后端实现（仅 USE_GLOG=ON 时使用）
├── test/
│   └── simple_test.cpp     # 测试程序
└── spdlog/                 # spdlog 子项目（未修改）
```

## 日志文件管理

### 默认日志目录

- 默认日志目录：`~/.my_log/<program_name>/`
- 日志文件名格式：`<program_name>_YYYY-MM-DD_HH-MM-SS.log`
- 每次创建新文件（包括轮转时）都会使用当前时间作为文件名后缀

### 文件大小限制

- 默认单个日志文件最大大小：10MB
- 当文件达到大小限制时，会自动创建新的带时间戳的日志文件
- 可通过 `LoggerOptions::max_log_size` 自定义文件大小限制

### 内存策略说明

**重要**：`max_log_size` 参数控制的是**磁盘文件大小**，不是内存缓冲大小。

- **同步模式**（`async_mode = false`）：
  - 日志消息立即写入磁盘，内存中只保留当前正在格式化的一条消息
  - 内存占用极小，但可能影响性能

- **异步模式**（`async_mode = true`，默认）：
  - 日志消息先放入内存队列（大小由 `async_queue_size` 控制，默认 32768 条）
  - 后台线程从队列取出消息并写入磁盘
  - 内存占用 = 队列大小 × 平均消息大小
  - 建议 `async_queue_size` 设置为 8192-65536 之间，根据日志量调整

## API 说明

### Logger 类方法

```cpp
// 初始化日志系统
static void Logger::Init(const LoggerOptions& opts);

// 关闭日志系统（程序退出前调用）
static void Logger::Shutdown();

// 检查是否已初始化
static bool Logger::IsInitialized();

// 获取程序名称
static std::string Logger::ProgramName();

// 设置日志等级
static void Logger::SetLogLevel(LogLevel level);

// 获取当前日志等级
static LogLevel Logger::GetLogLevel();
```

## 注意事项

1. **FATAL 日志**：`LOG(FATAL)` 会导致程序调用 `abort()` 终止
2. **线程安全**：spdlog 和 glog 后端都是线程安全的
3. **性能**：
   - 异步模式（默认）性能更好，适合高并发场景
   - 同步模式延迟更低，但可能影响主线程性能
4. **日志文件**：
   - 日志文件自动创建在 `~/.my_log/<program_name>/` 目录
   - 可通过 `log_dir` 选项自定义日志目录
   - 文件达到 `max_log_size` 限制时会自动轮转
5. **内存使用**：
   - 同步模式：内存占用极小
   - 异步模式：内存占用 = `async_queue_size` × 平均消息大小
   - 建议根据实际日志量调整 `async_queue_size`
6. **Coredump 日志收集（仅 glog 后端）**：
   - 当 `enable_coredump_log = true` 时，glog 会自动安装崩溃信号处理器
   - 捕获的信号包括：SIGSEGV（段错误）、SIGILL（非法指令）、SIGFPE（浮点异常）、SIGABRT（中止信号）
   - 程序崩溃时，会自动将堆栈跟踪信息写入日志文件，便于调试
   - 堆栈信息会写入到 `<program_name>.FATAL` 或 `<program_name>.ERROR` 日志文件中
   - 使用示例：
     ```cpp
     my::LoggerOptions opts;
     opts.enable_coredump_log = true;  // 启用 coredump 日志收集
     my::Logger::Init(opts);
     ```

