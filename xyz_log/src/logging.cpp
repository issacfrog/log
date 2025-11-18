#include "my_log.hpp"

// 在 spdlog 模式下，所有实现都在头文件中（header-only），源文件为空
// 在 glog 模式下，实现在这里

// If USE_GLOG, implement Logger::Init/Shutdown as wrappers
#if defined(USE_GLOG)
#include <mutex>
#include <filesystem>
#include <cstdlib>
namespace my {
static bool g_glog_inited = false;
static std::mutex g_glog_mtx;

void Logger::Init(const LoggerOptions& opts) {
    std::lock_guard<std::mutex> lk(g_glog_mtx);
    if (g_glog_inited) return;
    google::InitGoogleLogging(opts.program_name.c_str());
    
    // 确定日志目录：
    // - 如果 log_dir 为空，使用默认目录 ~/.my_log
    // - 如果 log_dir 不为空，使用传入的路径
    // - 无论哪种情况，都在最后加上程序名作为子目录
    std::string base_log_dir;
    if (opts.log_dir.empty()) {
        // 使用默认目录 ~/.my_log
        const char* home = std::getenv("HOME");
        if (home) {
            base_log_dir = std::string(home) + "/.my_log";
        } else {
            // 如果 HOME 环境变量不存在，使用当前目录
            base_log_dir = ".my_log";
        }
    } else {
        // 使用传入的路径
        base_log_dir = opts.log_dir;
    }
    
    // 在基础目录下添加程序名作为子目录
    std::string actual_log_dir = base_log_dir + "/" + opts.program_name;
    
    // 创建日志目录（如果不存在）
    if (!actual_log_dir.empty()) {
        try {
            std::filesystem::path log_path(actual_log_dir);
            if (!std::filesystem::exists(log_path)) {
                std::filesystem::create_directories(log_path);
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "Failed to create log directory %s: %s\n", 
                    actual_log_dir.c_str(), e.what());
            // 继续执行，glog 可能会尝试创建目录
        }
        
        // 设置 glog 的日志目录
        FLAGS_log_dir = actual_log_dir;
    }
    
    // 设置日志文件大小限制（glog 使用 MB 作为单位）
    FLAGS_max_log_size = static_cast<int>(opts.max_log_size / (1024 * 1024));
    if (FLAGS_max_log_size < 1) {
        FLAGS_max_log_size = 1;  // 最小 1MB
    }
    
    // 设置日志等级
    switch (opts.log_level) {
        case LogLevel::TRACE:
        case LogLevel::DEBUG:
            FLAGS_minloglevel = google::GLOG_INFO;  // glog 没有单独的 DEBUG/TRACE
            break;
        case LogLevel::INFO:
            FLAGS_minloglevel = google::GLOG_INFO;
            break;
        case LogLevel::WARNING:
            FLAGS_minloglevel = google::GLOG_WARNING;
            break;
        case LogLevel::ERROR:
            FLAGS_minloglevel = google::GLOG_ERROR;
            break;
        case LogLevel::FATAL:
            FLAGS_minloglevel = google::GLOG_FATAL;
            break;
        default:
            FLAGS_minloglevel = google::GLOG_ERROR;  // 默认使用 ERROR
    }
    
    FLAGS_alsologtostderr = opts.enable_console;
    
    // 是否开启崩溃log自动收集
    if (opts.enable_coredump_log) {
        google::InstallFailureSignalHandler();
    }
    
    g_glog_inited = true;
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lk(g_glog_mtx);
    if (!g_glog_inited) return;
    google::ShutdownGoogleLogging();
    g_glog_inited = false;
}

bool Logger::IsInitialized() { return g_glog_inited; }
std::string Logger::ProgramName() { return "glog_program"; }

void Logger::SetLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lk(g_glog_mtx);
    if (!g_glog_inited) return;
    
    // glog 使用不同的日志等级设置方式
    // 注意：glog 的等级设置与我们的枚举不完全对应
    switch (level) {
        case LogLevel::TRACE:
        case LogLevel::DEBUG:
            FLAGS_minloglevel = google::GLOG_INFO;  // glog 没有单独的 DEBUG/TRACE
            break;
        case LogLevel::INFO:
            FLAGS_minloglevel = google::GLOG_INFO;
            break;
        case LogLevel::WARNING:
            FLAGS_minloglevel = google::GLOG_WARNING;
            break;
        case LogLevel::ERROR:
            FLAGS_minloglevel = google::GLOG_ERROR;
            break;
        case LogLevel::FATAL:
            FLAGS_minloglevel = google::GLOG_FATAL;
            break;
        default:
            FLAGS_minloglevel = google::GLOG_INFO;
    }
}

LogLevel Logger::GetLogLevel() {
    std::lock_guard<std::mutex> lk(g_glog_mtx);
    if (!g_glog_inited) return LogLevel::INFO;
    
    // 将 glog 的等级转换为我们的枚举
    int glog_level = FLAGS_minloglevel;
    if (glog_level >= google::GLOG_FATAL) return LogLevel::FATAL;
    if (glog_level >= google::GLOG_ERROR) return LogLevel::ERROR;
    if (glog_level >= google::GLOG_WARNING) return LogLevel::WARNING;
    return LogLevel::INFO;  // glog 的 INFO 是默认最低等级
}

} // namespace my
#endif
