#include "include/threadpool.h"
#include <iostream>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <cassert>

using namespace std::chrono;

// ==================== 测试辅助函数 ====================
std::mutex print_mtx;
void safe_print(const std::string& msg) {
    std::lock_guard<std::mutex> lock(print_mtx);
    std::cout << msg << std::endl;
}

// ==================== 测试 1: 基本无返回值任务 ====================
void test_basic_submit() {

    safe_print("\n========== 测试 1: 基本无返回值任务 ==========");
    ThreadPool pool(4, QueMode::SPMC);
    
    std::atomic<int> counter{0};
    const int task_count = 100;
    
    for (int i = 0; i < task_count; ++i) {
        pool.submit([&counter, i]() {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        });
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    assert(counter.load() == task_count);
    safe_print("✓ 无返回值任务测试通过，执行任务数: " + std::to_string(counter.load()));
    safe_print(pool.getStatus());
}

// ==================== 测试 2: 有返回值任务 ====================
void test_submit_with_result() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 2: 有返回值任务 ==========");
    ThreadPool pool(4, QueMode::MPMC);
    
    std::vector<std::future<int>> futures;
    const int task_count = 20;
    
    for (int i = 0; i < task_count; ++i) {
        auto future = pool.submit([i]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return i * i;
        });
        futures.push_back(std::move(future));
    }
    
    int expected_sum = 0;
    for (int i = 0; i < task_count; ++i) {
        expected_sum += i * i;
    }
    
    int actual_sum = 0;
    for (auto& f : futures) {
        actual_sum += f.get();
    }
    
    assert(actual_sum == expected_sum);
    safe_print("✓ 有返回值任务测试通过，结果总和: " + std::to_string(actual_sum));
}

// ==================== 测试 3: SPMC 队列模式 ====================
void test_spmc_queue() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 3: SPMC 队列模式 ==========");
    ThreadPool pool(8, QueMode::SPMC);
    
    std::atomic<int> counter{0};
    const int task_count = 50000;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < task_count; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1);
        });
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    assert(counter.load() == task_count);
    safe_print("✓ SPMC 队列测试通过，任务数: " + std::to_string(task_count) + 
               ", 耗时: " + std::to_string(duration) + "ms");
}

// ==================== 测试 4: MPMC 队列模式 ====================
void test_mpmc_queue() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 4: MPMC 队列模式 ==========");
    ThreadPool pool(8, QueMode::MPMC);
    
    std::atomic<int> counter{0};
    const int task_count = 50000;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < task_count; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1);
        });
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    assert(counter.load() == task_count);
    safe_print("✓ MPMC 队列测试通过，任务数: " + std::to_string(task_count) + 
               ", 耗时: " + std::to_string(duration) + "ms");
}

// ==================== 测试 5: 单例模式 ====================
void test_singleton() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 5: 单例模式 ==========");
    
    auto& pool1 = ThreadPool::Pool(4, QueMode::SPMC);
    auto& pool2 = ThreadPool::Pool(4, QueMode::SPMC);
    
    assert(&pool1 == &pool2);
    safe_print("✓ 单例模式测试通过，两个实例地址相同");
    
    pool1.submit([]() {
        safe_print("  单例模式任务执行成功");
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ==================== 测试 6: 状态查询 ====================
void test_status_query() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 6: 状态查询 ==========");
    ThreadPool pool(4, QueMode::SPMC);
    
    safe_print("初始状态:");
    safe_print("  线程数: " + std::to_string(pool.getThreadCount()));
    safe_print("  总任务: " + std::to_string(pool.getTotalTasksCount()));
    safe_print("  已完成: " + std::to_string(pool.getCompletedCount()));
    safe_print("  失败数: " + std::to_string(pool.getFailedTasksCount()));
    safe_print("  队列大小: " + std::to_string(pool.getTasksQueueSize()));
    safe_print("  是否停止: " + std::to_string(pool.isStopped()));
    
    std::atomic<int> counter{0};
    for (int i = 0; i < 50; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    safe_print("\n执行中状态:");
    safe_print("  活跃线程: " + std::to_string(pool.getRunningThreadsCount()));
    safe_print("  总任务: " + std::to_string(pool.getTotalTasksCount()));
    safe_print("  已完成: " + std::to_string(pool.getCompletedCount()));
    safe_print("  队列大小: " + std::to_string(pool.getTasksQueueSize()));
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    safe_print("\n完成状态:");
    safe_print("  总任务: " + std::to_string(pool.getTotalTasksCount()));
    safe_print("  已完成: " + std::to_string(pool.getCompletedCount()));
    safe_print("  失败数: " + std::to_string(pool.getFailedTasksCount()));
    
    assert(pool.getTotalTasksCount() == 50);
    assert(pool.getCompletedCount() == 50);
    assert(pool.getFailedTasksCount() == 0);
    
    safe_print("✓ 状态查询测试通过");
}

// ==================== 测试 7: 异常处理 ====================
void test_exception_handling() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 7: 异常处理 ==========");
    ThreadPool pool(4, QueMode::SPMC);
    
    std::atomic<int> normal_counter{0};
    
    for (int i = 0; i < 10; ++i) {
        if (i&&i % 3 == 0) {
            pool.submit([]() {
                throw std::runtime_error("测试异常");
            });
        } else {
            pool.submit([&normal_counter]() {
                normal_counter.fetch_add(1);
            });
        }
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    safe_print("  正常任务完成: " + std::to_string(normal_counter.load()));
    safe_print("  失败任务: " + std::to_string(pool.getFailedTasksCount()));
    
    assert(normal_counter.load() == 7);
    assert(pool.getFailedTasksCount() == 3);
    
    safe_print("✓ 异常处理测试通过");
}

// ==================== 测试 8: 带参数任务 ====================
void test_task_with_args() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 8: 带参数任务 ==========");
    ThreadPool pool(4, QueMode::MPMC);
    
    std::atomic<int> sum{0};
    
    auto add = [&sum](int a, int b) {
        sum.fetch_add(a + b);
    };
    
    auto multiply = [](int a, int b) -> int {
        return a * b;
    };
    
    pool.submit(add, 10, 20);
    pool.submit(add, 30, 40);
    
    auto future1 = pool.submit(multiply, 5, 6);
    auto future2 = pool.submit(multiply, 7, 8);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    assert(sum.load() == 100);

    int result1 = future1.get();
    int result2 = future2.get();
    assert(result1 == 30);
    assert(result2 == 56);

    safe_print("✓ 带参数任务测试通过，sum: " + std::to_string(sum.load()) +
               ", 乘法结果: " + std::to_string(result1) + ", " + std::to_string(result2));
}

// ==================== 测试 9: 高并发压力测试 ====================
void test_stress() {
    safe_print("\n========== 测试 9: 高并发压力测试 ==========");
    ThreadPool pool(16, QueMode::MPMC);
    
    std::atomic<int> counter{0};
    const int task_count = 10000;
    
    auto start = high_resolution_clock::now();
    
    for (int i = 0; i < task_count; ++i) {
        pool.submit([&counter]() {
            counter.fetch_add(1);
        });
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    safe_print("  任务数: " + std::to_string(task_count));
    safe_print("  完成数: " + std::to_string(counter.load()));
    safe_print("  耗时: " + std::to_string(duration) + "ms");
    safe_print("  吞吐量: " + std::to_string(task_count * 1000 / duration) + " 任务/秒");
    
    assert(counter.load() == task_count);
    safe_print("✓ 压力测试通过");
}

// ==================== 测试 10: 优雅关闭 ====================
void test_graceful_shutdown() {
    safe_print("\n========== 测试 10: 优雅关闭 ==========");
    
    {
        ThreadPool pool(4, QueMode::SPMC);
        
        std::atomic<int> counter{0};
        for (int i = 0; i < 20; ++i) {
            pool.submit([&counter]() {
                counter.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            });
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        safe_print("  关闭前完成: " + std::to_string(counter.load()));
    }
    
    safe_print("✓ 优雅关闭测试通过（pool 已析构）");
}

// ==================== 主函数 ====================
int main() {
    safe_print("========================================");
    safe_print("   ThreadPool 测试程序");
    safe_print("========================================");
    
    try {
        test_basic_submit();
        test_submit_with_result();
        test_spmc_queue();
        test_mpmc_queue();
        test_singleton();
        test_status_query();
        test_exception_handling();
        test_task_with_args();
        test_stress();
        test_graceful_shutdown();
        
        safe_print("\n========================================");
        safe_print("   ✓ 所有测试通过！");
        safe_print("========================================");
        
    } catch (const std::exception& e) {
        safe_print("\n✗ 测试失败: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
