#pragma once
/**
 * @file my_log.hpp
 * @author zhouchao (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-11-18
 * 对glog和spdlog进行封装，提供统一的日志接口
 * spdlog默认不支持滚动名称日志，需要自定义sink相关内容，自测耗时会增加
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <string>
#include <cstdint>

#if defined(USE_GLOG)
  #include <glog/logging.h>
  // 对TRACE和DEBUG级别LOG进行重映射
  #define LOG_TRACE  LOG(INFO)
  #define LOG_DEBUG  LOG(INFO)
#else
  // Header-only implementation for SPDLOG backend
  #include <spdlog/spdlog.h>
  #include <spdlog/sinks/basic_file_sink.h>
  #include <spdlog/sinks/stdout_color_sinks.h>
  #include <spdlog/async.h>
  #include <spdlog/async_logger.h>
  #include <spdlog/sinks/rotating_file_sink.h>
  #include <spdlog/fmt/bundled/format.h>
  #include <spdlog/fmt/bundled/chrono.h>
  #include <sstream>
  #include <chrono>
  #include <thread>
  #include <cstring>
  #include <filesystem>
  #include <cstdlib>  
#endif

namespace my {

enum class LogLevel { TRACE=0, DEBUG=1, INFO=2, WARNING=3, ERROR=4, FATAL=5 };

struct LoggerOptions {
    std::string program_name = "my_app";
    std::string log_dir = "";      // 空字符串表示使用默认目录 ~/.my_log
    bool enable_console = false;   // 默认不向控制台输出，开启时日志文件和控制台都会有数据
    bool async_mode = true;       // for spdlog 默认异步模式 性能提升
    bool multi_thread = true;      // use mt sinks if true
    std::size_t async_queue_size = 32768; // 注意缓存大小，与日志量大小相关，建议8192-65536
    LogLevel log_level = LogLevel::ERROR;  // 默认日志等级
    std::size_t max_log_size = 10 * 1024 * 1024;  // 默认最大日志文件大小 10MB (单位: 字节)
    bool enable_coredump_log = true;  // 是否启用 coredump 日志收集（仅 glog 后端有效，默认 true）
};

class Logger {
public:
    static void Init(const LoggerOptions& opts);

    static void Shutdown();

    static bool IsInitialized();

    static std::string ProgramName();

    static void SetLogLevel(LogLevel level);

    static LogLevel GetLogLevel();

private:
    Logger() = delete;
    ~Logger() = delete;
};

}

#if not defined(USE_GLOG)
namespace my {
// Internal state (header-only, using static storage)
struct LoggerState {
    LoggerOptions opts;
    bool initialized = false;
    std::shared_ptr<spdlog::logger> logger;
    std::shared_ptr<spdlog::async_logger> async_logger;
    std::mutex mtx;
};

inline LoggerState& GetLoggerState() {
    static LoggerState g_state;
    return g_state;
}

// Logger implementation (header-only)
inline void Logger::Init(const LoggerOptions& opts) {
    auto& state = GetLoggerState();
    std::lock_guard<std::mutex> lk(state.mtx);
    if (state.initialized) return;
    state.opts = opts;

    try {
        std::vector<spdlog::sink_ptr> sinks;
        // 默认使用 /home/username/.my_log 作为日志目录
        std::string base_log_dir;
        if (opts.log_dir.empty()) {
            const char* home = std::getenv("HOME");
            if (home) {
                base_log_dir = std::string(home) + "/.my_log";
            } else {
                base_log_dir = ".my_log";
            }
        } else {
            base_log_dir = opts.log_dir;
        }
        
        // 支持根据程序名称创建子目录
        std::string actual_log_dir = base_log_dir + "/" + opts.program_name;
        
        if (!actual_log_dir.empty()) {
            try {
                std::filesystem::path log_path(actual_log_dir);
                if (!std::filesystem::exists(log_path)) {
                    std::filesystem::create_directories(log_path);
                }
            } catch (const std::exception& e) {
                fprintf(stderr, "Failed to create log directory %s: %s\n", 
                        actual_log_dir.c_str(), e.what());
            }
            
            // 生成带时间戳的文件名: program_name_YYYY-MM-DD_HH-MM-SS.log
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
            localtime_r(&time_t, &tm_buf);
            
            char time_str[32];
            std::snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d_%02d-%02d-%02d",
                         tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                         tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
            
            // 使用 rotating_file_sink 支持文件大小限制和轮转
            // 使用 spdlog 的 MaxFiles 常量（200000），基本不限制文件数量
            std::string file = actual_log_dir + "/" + opts.program_name + "_" + time_str + ".log";
            constexpr std::size_t unlimited_files = spdlog::sinks::rotating_file_sink_mt::MaxFiles;
            if (opts.multi_thread) {
                sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    file, opts.max_log_size, unlimited_files, true));
            } else {
                sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_st>(
                    file, opts.max_log_size, unlimited_files, true));
            }
        }

        // console sink
        if (opts.enable_console) {
            if (opts.multi_thread) {
                sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
            } else {
                sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_st>());
            }
        }

        if (opts.async_mode) {
            spdlog::init_thread_pool(opts.async_queue_size, 1);
            state.logger = std::make_shared<spdlog::async_logger>(
                "my_async_logger",
                sinks.begin(), sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
            spdlog::set_default_logger(state.logger);
        } else {
            state.logger = std::make_shared<spdlog::logger>("my_logger", sinks.begin(), sinks.end());
            spdlog::set_default_logger(state.logger);
        }

        // 设置日志形式
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        // 设置日志等级
        spdlog::level::level_enum spdlog_level;
        switch (opts.log_level) {
            case LogLevel::TRACE: spdlog_level = spdlog::level::trace; break;
            case LogLevel::DEBUG: spdlog_level = spdlog::level::debug; break;
            case LogLevel::INFO:  spdlog_level = spdlog::level::info; break;
            case LogLevel::WARNING: spdlog_level = spdlog::level::warn; break;
            case LogLevel::ERROR: spdlog_level = spdlog::level::err; break;
            case LogLevel::FATAL: spdlog_level = spdlog::level::critical; break;
            default: spdlog_level = spdlog::level::info;
        }
        spdlog::set_level(spdlog_level);
        if (state.logger) {
            state.logger->set_level(spdlog_level);
        }
        
        state.initialized = true;

    } catch (const spdlog::spdlog_ex& ex) {
        fprintf(stderr, "spdlog init failed: %s\n", ex.what());
    }
}

inline void Logger::Shutdown() {
    auto& state = GetLoggerState();
    std::lock_guard<std::mutex> lk(state.mtx);
    if (!state.initialized) return;
    spdlog::shutdown();
    state.initialized = false;
}

inline bool Logger::IsInitialized() {
    return GetLoggerState().initialized;
}

inline std::string Logger::ProgramName() {
    return GetLoggerState().opts.program_name;
}

inline void Logger::SetLogLevel(LogLevel level) {
    auto& state = GetLoggerState();
    std::lock_guard<std::mutex> lk(state.mtx);
    if (!state.initialized) return;
    
    state.opts.log_level = level;
    // 日志等级映射
    spdlog::level::level_enum spdlog_level;
    switch (level) {
        case LogLevel::TRACE: spdlog_level = spdlog::level::trace; break;
        case LogLevel::DEBUG: spdlog_level = spdlog::level::debug; break;
        case LogLevel::INFO:  spdlog_level = spdlog::level::info; break;
        case LogLevel::WARNING: spdlog_level = spdlog::level::warn; break;
        case LogLevel::ERROR: spdlog_level = spdlog::level::err; break;
        case LogLevel::FATAL: spdlog_level = spdlog::level::critical; break;
        default: spdlog_level = spdlog::level::info;
    }
    
    spdlog::set_level(spdlog_level);
    if (state.logger) {
        state.logger->set_level(spdlog_level);
    }
}

inline LogLevel Logger::GetLogLevel() {
    auto& state = GetLoggerState();
    std::lock_guard<std::mutex> lk(state.mtx);
    return state.opts.log_level;
}

// 模型生成，用于优化日志性能具体没看懂
// FastLogStream: lightweight zero-copy builder (header-only)
class FastLogStream {
    public:
        FastLogStream(const char* file, int line, LogLevel lvl);
        ~FastLogStream();
        
        // Provide stream-like operator<<
        template<typename T>
        FastLogStream& operator<<(const T& v) {
            append(v);
            return *this;
        }
        
        // for c-string
        FastLogStream& operator<<(const char* s) {
            append(s);
            return *this;
        }
        
        // access underlying stream builder
        std::string_view view() const;
    
        // used by macro: returns an object with operator<<
        FastLogStream& stream() { return *this; }
    
    private:
        // non-copyable
        FastLogStream(const FastLogStream&) = delete;
        FastLogStream& operator=(const FastLogStream&) = delete;
    
        // append specializations
        void append(const char* s);
        template<typename T>
        void append(const T& v) {
            append_impl(v);
        }
        template<typename T>
        void append_impl(const T& v);
    
        // internal buffer
        struct Impl;
        Impl* impl_;
};

// FastLogStream: lightweight zero-copy builder (header-only)
struct FastLogStream::Impl {
    fmt::memory_buffer buf;
    LogLevel lvl;
    const char* file;
    int line;
};

inline FastLogStream::FastLogStream(const char* file, int line, LogLevel lvl)
    : impl_(new Impl{fmt::memory_buffer{}, lvl, file, line})
{
    //  // prefix with file:line
    //  fmt::format_to(std::back_inserter(impl_->buf), "[{}:{}] ", file, line);
    // 从完整路径中提取文件名（去掉目录路径）
    const char* filename = file;
    const char* last_slash = std::strrchr(file, '/');
    if (last_slash) {
        filename = last_slash + 1;
    }
    const char* last_backslash = std::strrchr(filename, '\\');
    if (last_backslash) {
        filename = last_backslash + 1;
    }
    
    // prefix with filename:line (只显示文件名，不显示完整路径)
    fmt::format_to(std::back_inserter(impl_->buf), "[{}:{}] ", filename, line);
}

inline FastLogStream::~FastLogStream() {
    auto& state = GetLoggerState();
    if (!state.initialized) {
        // fallback: print to stderr
        fmt::string_view s(impl_->buf.data(), impl_->buf.size());
        fprintf(stderr, "%.*s\n", (int)s.size(), s.data());
        delete impl_;
        return;
    }

    // convert level
    spdlog::level::level_enum sl;
    switch (impl_->lvl) {
        case LogLevel::TRACE: sl = spdlog::level::trace; break;
        case LogLevel::DEBUG: sl = spdlog::level::debug; break;
        case LogLevel::INFO:  sl = spdlog::level::info; break;
        case LogLevel::WARNING: sl = spdlog::level::warn; break;
        case LogLevel::ERROR: sl = spdlog::level::err; break;
        case LogLevel::FATAL: sl = spdlog::level::critical; break;
        default: sl = spdlog::level::info;
    }

    // send to spdlog
    fmt::string_view s(impl_->buf.data(), impl_->buf.size());
    spdlog::log(sl, s);

    // For FATAL, abort
    if (impl_->lvl == LogLevel::FATAL) {
        spdlog::default_logger()->flush();
        // give time to flush
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::abort();
    }

    delete impl_;
}

inline void FastLogStream::append(const char* s) {
    if (!s) return;
    impl_->buf.append(s, s + std::strlen(s));
}

template<typename T>
inline void FastLogStream::append_impl(const T& v) {
    // use fmt to format to buffer
    fmt::format_to(std::back_inserter(impl_->buf), "{}", v);
}

// Explicit template instantiations for common types
// 注意：显式实例化不能使用 inline 关键字（C++ 标准要求）
template void FastLogStream::append_impl<int>(const int&);
template void FastLogStream::append_impl<long>(const long&);
template void FastLogStream::append_impl<long long>(const long long&);
template void FastLogStream::append_impl<unsigned>(const unsigned&);
template void FastLogStream::append_impl<unsigned long>(const unsigned long&);
template void FastLogStream::append_impl<unsigned long long>(const unsigned long long&);
template void FastLogStream::append_impl<float>(const float&);
template void FastLogStream::append_impl<double>(const double&);
template void FastLogStream::append_impl<std::string>(const std::string&);

inline std::string_view FastLogStream::view() const {
    return std::string_view(impl_->buf.data(), impl_->buf.size());
}

} // namespace my

// Macro that creates a temporary FastLogStream with file/line/level
#define LOG(level) ::my::FastLogStream(__FILE__, __LINE__, ::my::LogLevel::level).stream()

#endif // !USE_GLOG
