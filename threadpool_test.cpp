#include "src/threadpool.h"
#include "Task_Queue.h"
#include <iostream>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <deque>
#include <thread>
#include <cstdint>
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
    ThreadPool pool(4, QueueMode::SPMC);
    
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
    ThreadPool pool(4, QueueMode::MPMC);
    
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
    ThreadPool pool(8, QueueMode::SPMC);
    
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
    ThreadPool pool(8, QueueMode::MPMC);
    
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
    
    auto& pool1 = ThreadPool::Pool(4, QueueMode::SPMC);
    auto& pool2 = ThreadPool::Pool(4, QueueMode::SPMC);
    
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
    ThreadPool pool(4, QueueMode::SPMC);
    
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
    ThreadPool pool(4, QueueMode::SPMC);
    
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
    ThreadPool pool(4, QueueMode::MPMC);
    
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
    ThreadPool pool(16, QueueMode::MPMC);
    
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
        ThreadPool pool(4, QueueMode::SPMC);
        
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

// ==================== 测试 11: SPSC 有界缓冲区基础语义 ====================
void test_spsc_bounded_basic() {
    safe_print("\n========== 测试 11: SPSC 有界缓冲区基础语义 ==========");

    // 小容量(8) 便于快速触发满载与回绕
    BoundedQueue<uint32_t, 8> queue;

    // --- 空队列 ---
    uint32_t out = 0;
    assert(queue.try_pop(out) == false);
    safe_print("✓ 空队列 try_pop 返回 false ");
    assert( queue.size() == 0 ); 
    safe_print("✓ 空队列 size 为 0");

    // --- FIFO 顺序 ---
    for (uint32_t i = 0; i < 5; ++i) {
        assert(queue.try_push(i) == true);
    }
	assert( queue.size() == 5 );
	safe_print( "✓ FIFO 顺序测试: 入队 5 条数据，size 为 5" );
    for (uint32_t i = 0; i < 5; ++i) {
        assert(queue.try_pop(out) == true);
        assert(out == i);
		assert( queue.size() == 5 - i -1 );
    }

    assert(queue.try_pop(out) == false);
	assert( queue.size() == 0 );
    safe_print("✓ FIFO 顺序正确");


    // --- 满载: 容量为 8，塞满第 9 个必须失败 ---
    for (uint32_t i = 0; i < BoundedQueue<uint32_t, 8>::capacity_; ++i) {
        assert(queue.try_push(i) == true);
    }
    assert(queue.try_push(999) == false);
    safe_print("✓ 满载时 try_push 返回 false（背压语义），容量: " +
               std::to_string(BoundedQueue<uint32_t, 8>::capacity_));

    // --- 回绕: 连续多轮 push/pop，跨越缓冲区末尾 ---
    // 此时缓冲区满且内容为 0..7。用参考模型(std::deque)比对每一轮读出的值，
    // 这样索引回绕(8 -> 0 -> 8 ...)时任何错位都会被捕获。
    std::deque<uint32_t> model{0, 1, 2, 3, 4, 5, 6, 7};
    for (uint32_t base = 0; base < 100; ++base) {
        assert(queue.try_push(base) == false);   // 仍是满的
        assert((model.size() == BoundedQueue<uint32_t, 8>::capacity_));

        assert(queue.try_pop(out) == true);
        assert(out == model.front());            // 与参考模型逐条比对
        model.pop_front();

        assert(queue.try_push(base) == true);
        model.push_back(base);
    }
    // 100 轮后缓冲区解析顺序应为 92..99
    for (uint32_t base = 92; base < 100; ++base) {
        assert(queue.try_pop(out) == true);
        assert(out == base);
    }
    assert(queue.try_pop(out) == false);
    safe_print("✓ 索引回绕测试通过（100 轮跨边界读写，与参考模型一致）");

    // --- shutdown 后拒绝写入，但剩余数据仍可读出 ---
    assert(queue.try_push(1) == true);
    queue.shutdown();
    assert(queue.is_stopping() == true);
    assert(queue.try_push(2) == false);
    assert(queue.try_pop(out) == true && out == 1);
    safe_print("✓ shutdown 后拒绝写入、存量数据仍可消费");
}

// ==================== 测试 12: SPSC 单生产者单消费者并发 ====================
void test_spsc_concurrent() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 12: SPSC 单生产者单消费者并发 ==========");

    constexpr size_t kCapacity = 1024;
    constexpr uint32_t kCount = 1'000'000;   // 每个值唯一，可校验丢失/乱序/重复

    BoundedQueue<uint32_t, kCapacity> queue;

    std::atomic<bool> producer_done{false};
    std::atomic<size_t> push_retries{0};
    std::atomic<size_t> consumed{0};

    auto start = high_resolution_clock::now();

    // 消费者：严格按 0,1,2,... 校验顺序与完整性
    std::thread consumer([&]() {
        uint32_t expected = 0;
        uint32_t value = 0;
        while (expected < kCount) {
            if (!queue.try_pop(value)) {
                if (producer_done.load(std::memory_order_acquire)) {
                    // 生产者已结束，仍读不到数据 -> 说明有数据丢失
                    break;
                }
                std::this_thread::yield();
                continue;
            }
            assert(value == expected); // 顺序错误/数据错位
            ++expected;
            consumed.store(expected, std::memory_order_relaxed);
        }
        consumed.store(expected, std::memory_order_release);
        assert(expected == kCount);
    });

    // 生产者：满则重试（背压），统计被拒次数
    std::thread producer([&]() {
        for (uint32_t i = 0; i < kCount; ++i) {
            while (!queue.try_push(i)) {
                push_retries.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();

    assert(consumed.load() == kCount);

    safe_print("  传输数据: " + std::to_string(kCount) + " 条 (全部唯一)");
    safe_print("  消费数据: " + std::to_string(consumed.load()) + " 条，顺序与完整性校验通过");
    safe_print("  背压重试: " + std::to_string(push_retries.load()) + " 次");
    safe_print("  耗时: " + std::to_string(duration) + "ms");
    safe_print("  吞吐量: " + std::to_string(kCount * 1000 / (duration ? duration : 1)) + " 条/秒");
    safe_print("✓ SPSC 并发测试通过（无丢失、无乱序、无重复）");
}

// ==================== 测试 13: SPSC 无背压时的丢弃行为 ====================
void test_spsc_drop_on_full() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    safe_print("\n========== 测试 13: SPSC 满载丢弃统计 ==========");

    constexpr size_t kCapacity = 64;
    BoundedQueue<int, kCapacity> queue;

    // 生产者不重试，直接把满载后的写入请求记为丢弃
    constexpr int kAttempts = 10'000;
    int accepted = 0;
    int dropped = 0;
    for (int i = 0; i < kAttempts; ++i) {
        if (queue.try_push(i)) ++accepted;
        else ++dropped;
    }

    safe_print("  尝试写入: " + std::to_string(kAttempts));
    safe_print("  成功入队: " + std::to_string(accepted));
    safe_print("  被拒绝:   " + std::to_string(dropped));

    // 无消费者时成功数必须恰好等于容量，其余全部被拒
    assert(accepted == static_cast<int>(kCapacity));
    assert(dropped == kAttempts - static_cast<int>(kCapacity));

    // 校验保留的是最早写入的 kCapacity 条（先进先出，不覆盖旧数据）
    int out = 0;
    for (size_t i = 0; i < kCapacity; ++i) {
        assert(queue.try_pop(out) == true);
        assert(out == static_cast<int>(i));
    }
    assert(queue.try_pop(out) == false);

    safe_print("✓ 满载丢弃测试通过：只保留最早的 64 条，后续写入被拒绝（不覆盖旧数据）");
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
        test_spsc_bounded_basic();
        test_spsc_concurrent();
        test_spsc_drop_on_full();
        
        safe_print("\n========================================");
        safe_print("   ✓ 所有测试通过！");
        safe_print("========================================");
        
    } catch (const std::exception& e) {
        safe_print("\n✗ 测试失败: " + std::string(e.what()));
        return 1;
    }
    
    return 0;
}
