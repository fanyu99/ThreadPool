#pragma once
#include <sstream>
#include <iomanip>
#include <utility>
#include <iostream>
#include <future>
#include <thread>
#include <mutex>
#include <functional>
#include <memory>
#include <queue>
#include <atomic>
#include "../../my_socket/logger/include/logger.h" //日志类
#include <condition_variable>
#include <vector>
#include <tuple>
// TODO:
//1. 将队列进行封装 : 分为一对多 / 多对多
//2. 重构线程池后进行集成
class ThreadPool
{
private:
    std::vector<std::thread> threadpool;         // 线程池
    std::queue<std::function<void()>> taskqueue; // 任务队列
    mutable std::mutex mtx;                      // mutble使其能在const函数中使用
    std::condition_variable cv;
    std::atomic_bool stop;              // 停止标志
    size_t threadcount;                 // 线程数量
    void workerThread();                // 每个线程的工作函数,无限循环,直至停止
    std::atomic_size_t totalTasks;     // 总任务数
    std::atomic_size_t completeTasks;  // 已经完成任务数
    std::atomic_size_t failedTasks;    // 失败任务数
    std::atomic_size_t runningThreads; // 活跃线程数
    // 任务入列
    template <typename F, typename... Args>
    void entaskque(F &&f, Args &&...args)
    {
        ++totalTasks;
        // 使用元组进行参数绑定,使用apply执行函数
        auto task = [func = std::forward<F>(f), argu = std::make_tuple(std::forward<Args>(args)...)]() mutable
        { std::apply(func, argu); };
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (stop)
            {
                Logger::instance().warning("ThreadPool is stopped!");
                throw std::runtime_error("ThreadPool is stopped!\n");
            }
            taskqueue.emplace(std::move(task)); // lambda不可拷贝,使用移动语义
        }
        cv.notify_one(); // 唤醒一个进程
    }

public:
    // 单例模式
    ThreadPool();
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    explicit ThreadPool(size_t threadnum);
    ~ThreadPool();
    static ThreadPool &Pool(size_t threadcount=std::thread::hardware_concurrency()); // 获取singleton

    // 普通函数提交
    template <typename F, typename... Args>
    void submit(F &&f, Args &&...args)
    {
        this->entaskque(f, args...);
        Logger::instance().debug("Task submitted,total tasks: " + std::to_string(getTotalTasksCount()));
    }
    // 有返回值(返回future异步获取结果)函数提交
    template <typename F, typename... Args>
    auto submit_result(F &&f, Args &&...args)
    {
        ++totalTasks;
        using return_type = decltype(f(args...)); // 返回值类型
        // 使用lambda函数,使用tuple绑定参数,apply执行包装函数
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            [func = std::forward<F>(f), arg = std::make_tuple(std::forward<Args>(args)...)]() mutable
            { return std::apply(func, arg); });               // 使用智能指针确保lambda执行时仍然有效
        std::future<return_type> res_future = task->get_future(); // 异步获取结果
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (stop)
            {
                throw std::runtime_error("ThreadPool is stopped!\n");
            }
            taskqueue.emplace([task]()
                              { (*task)(); });
        }
        cv.notify_one(); // 入列后唤醒一个进程
        Logger::instance().debug("Task submitted with result,total tasks: " + std::to_string(getTotalTasksCount()));
        return res_future; // 返回future对象,后续异步获取结果
    }
    // 状态查询
    size_t getThreadCount() const;         // 返回线程总数
    size_t getRunningThreadsCount() const; // 返回活跃线程
    size_t getTotalTasksCount() const;     // 返回总任务数
    size_t getCompletedCount() const;      // 返回完成任务数
    size_t getFailedTasksCount() const;    // 返回失败任务数
    size_t getTasksQueueSize() const;      // 返回任务队列大小
    bool isStopped() const;                // 返回是否停止
    std::string getStatus() const;         // 返回详细的格式化状态信息
};