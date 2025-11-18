#include "my_log.hpp"
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>

// 原生 spdlog 测试（仅在非 glog 模式下）
#if not defined(USE_GLOG)
#include <spdlog/spdlog.h>
#endif

// 性能测试工具函数
template<typename Func>
double benchmark(const std::string& name, Func&& func, int iterations = 100000) {
    // 预热
    for (int i = 0; i < 1000; ++i) {
        func();
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_time_us = static_cast<double>(duration.count()) / iterations;
    
    std::cout << name << ": " << avg_time_us << " us/op (total: " 
              << duration.count() << " us for " << iterations << " iterations)" << std::endl;
    
    return avg_time_us;
}

int main(int argc, char** argv) {
    my::LoggerOptions opts;
    opts.program_name = "my_demo";
    // opts.log_dir = "./logs";
    opts.enable_console = false;   // 开启控制台输出，查看时间戳格式
    opts.async_mode = true;      // 同步模式，更公平的比较
    opts.multi_thread = true;
    opts.log_level = my::LogLevel::DEBUG;  // 设置日志等级为 DEBUG
    opts.max_log_size = 10 * 1024 * 1024;  // 10MB
    my::Logger::Init(opts);
    
    // 测试日志等级设置
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "测试日志等级设置功能" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "当前日志等级: " << static_cast<int>(my::Logger::GetLogLevel()) << " (DEBUG)" << std::endl;
    LOG(TRACE) << "这条 TRACE 消息应该被过滤（等级是 DEBUG）";
    LOG(DEBUG) << "这条 DEBUG 消息应该显示";
    LOG(INFO) << "这条 INFO 消息应该显示";
    LOG(WARNING) << "这条 WARNING 消息应该显示";
    
    my::Logger::SetLogLevel(my::LogLevel::WARNING);
    std::cout << std::endl << "设置日志等级为 WARNING" << std::endl;
    LOG(DEBUG) << "这条 DEBUG 消息应该被过滤";
    LOG(INFO) << "这条 INFO 消息应该被过滤";
    LOG(WARNING) << "这条 WARNING 消息应该显示";
    LOG(ERROR) << "这条 ERROR 消息应该显示";
    
    my::Logger::SetLogLevel(my::LogLevel::INFO);
    std::cout << std::endl << "恢复日志等级为 INFO" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "性能测试：我们的 LOG 宏 vs 原生 spdlog" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    const int iterations = 100000;
    const std::string test_msg = "Test message with value: ";
    int test_value = 42;

#if not defined(USE_GLOG)
    // 测试 1: 我们的 LOG 宏
    std::cout << "测试 1: 使用我们的 LOG(INFO) 宏" << std::endl;
    double our_log_time = benchmark("  LOG(INFO)", [&]() {
        LOG(INFO) << test_msg << test_value;
    }, iterations);

    // 测试 2: 原生 spdlog（使用格式化字符串）
    std::cout << std::endl << "测试 2: 原生 spdlog::info (格式化字符串)" << std::endl;
    double spdlog_fmt_time = benchmark("  spdlog::info", [&]() {
        spdlog::info("{} {}", test_msg, test_value);
    }, iterations);

    // 测试 3: 原生 spdlog（使用 source_loc）
    std::cout << std::endl << "测试 3: 原生 spdlog::log (带 source_loc)" << std::endl;
    double spdlog_loc_time = benchmark("  spdlog::log(loc)", [&]() {
        spdlog::source_loc loc{__FILE__, __LINE__, nullptr};
        spdlog::log(loc, spdlog::level::info, "{} {}", test_msg, test_value);
    }, iterations);

    // 测试 4: 原生 spdlog（直接 log，无 source_loc）
    std::cout << std::endl << "测试 4: 原生 spdlog::log (无 source_loc)" << std::endl;
    double spdlog_simple_time = benchmark("  spdlog::log", [&]() {
        spdlog::log(spdlog::level::info, "{} {}", test_msg, test_value);
    }, iterations);

    // 性能对比
    std::cout << std::endl << "========================================" << std::endl;
    std::cout << "性能对比结果：" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "我们的 LOG(INFO) 宏:        " << our_log_time << " us/op" << std::endl;
    std::cout << "spdlog::info:              " << spdlog_fmt_time << " us/op" << std::endl;
    std::cout << "spdlog::log(loc):          " << spdlog_loc_time << " us/op" << std::endl;
    std::cout << "spdlog::log (无 loc):      " << spdlog_simple_time << " us/op" << std::endl;
    std::cout << std::endl;
    
    double overhead = our_log_time - spdlog_fmt_time;
    double overhead_percent = (overhead / spdlog_fmt_time) * 100.0;
    std::cout << "性能开销: " << overhead << " us/op (" 
              << (overhead_percent >= 0 ? "+" : "") << overhead_percent << "%)" << std::endl;
    std::cout << std::endl;

    // 测试 5: 多线程性能测试
    std::cout << "========================================" << std::endl;
    std::cout << "多线程性能测试 (4 线程, 每个线程 " << iterations << " 次)" << std::endl;
    std::cout << "========================================" << std::endl;
    
    auto start_mt = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads;
    const int num_threads = 4;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterations; ++i) {
                LOG(INFO) << "Thread " << t << " message " << i;
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_mt = std::chrono::high_resolution_clock::now();
    auto duration_mt = std::chrono::duration_cast<std::chrono::microseconds>(end_mt - start_mt);
    double avg_time_mt = static_cast<double>(duration_mt.count()) / (num_threads * iterations);
    
    std::cout << "多线程平均时间: " << avg_time_mt << " us/op" << std::endl;
    std::cout << "总时间: " << duration_mt.count() << " us" << std::endl;
    std::cout << std::endl;

#else
    // glog 模式下只测试我们的 LOG 宏
    std::cout << "测试: 使用我们的 LOG(INFO) 宏 (glog 后端)" << std::endl;
    double our_log_time = benchmark("  LOG(INFO)", [&]() {
        LOG(INFO) << test_msg << test_value;
    }, iterations);
    std::cout << std::endl;
#endif

    // 基本功能测试
    std::cout << "========================================" << std::endl;
    std::cout << "基本功能测试" << std::endl;
    std::cout << "========================================" << std::endl;
    LOG(INFO) << "Hello world " << 123;
    LOG(WARNING) << "Warn value: " << 3.14;
    LOG(ERROR) << "Something bad";
    // LOG(FATAL) << "This is fatal, will abort";

    my::Logger::Shutdown();
    return 0;
}
