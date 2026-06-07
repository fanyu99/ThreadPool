#pragma once
#include <sstream>
#include <iomanip>
#include <utility>
#include <iostream>
#include <future>
#include <thread>
#include <functional>
#include <memory>
#include <queue>
#include <atomic>
#include <vector>
#include "logger.h" //日志类
#include "Task_Queue.h"               //封装的任务队列类
#include <condition_variable>
#include <tuple>
#include <stdexcept>
class ThreadPool
{
private:
    std::vector<std::thread> threadpool;   // 线程池
    std::unique_ptr<Task_Queue> taskqueue; // 任务队列指针
    std::condition_variable cv;            // 条件变量用于通知线程取任务
    std::atomic_bool stop;                 // 停止标志
    std::atomic_size_t threadcount;        // 线程数量
    void workerThread();                   // 每个线程的工作函数,无限循环,直至停止
    std::mutex mtx;                       // 互斥锁,用于保护任务队列的访问
    std::atomic_size_t totalTasks;         // 总任务数
    std::atomic_size_t completeTasks;      // 已经完成任务数
    std::atomic_size_t failedTasks;        // 失败任务数
    std::atomic_size_t runningThreads;     // 活跃线程数
    QueueMode quemode;                       // 当前的任务队列模式

public:
    // 单例模式
    ThreadPool(QueueMode mode = QueueMode::SPMC);

    ThreadPool(const ThreadPool &) = delete;

    ThreadPool &operator=(const ThreadPool &) = delete;
    explicit ThreadPool(size_t threadnum, QueueMode mode = QueueMode::SPMC);

    ~ThreadPool();

    static ThreadPool &Pool(size_t threadcount = std::thread::hardware_concurrency(), QueueMode mode = QueueMode::SPMC); // 获取singleton

    // 任务提交到队列
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args)
    {
        using return_type = std::invoke_result_t<F, Args...>;
        {
            std::unique_lock<std::mutex> lock(mtx);
            if (stop.load())
            {
                throw std::runtime_error("ThreadPool is stopped!\n");
            }
        }
        // 如果没有返回值,直接加入队列
        if constexpr (std::is_void_v<return_type>)
        {
            // 使用元组进行参数绑定,使用apply执行函数
            auto task = [func = std::forward<F>(f), argu = std::make_tuple(std::forward<Args>(args)...)]() mutable
            { std::apply(func, argu); };
            if (!taskqueue->push(std::move(task))) // lambda不可拷贝,使用移动语义
            {
                Logger::instance().error("Task submission failed!");
                throw std::runtime_error("Task submit failed!\n");
            }
            else
            {
                totalTasks.fetch_add(1);
                Logger::instance().debug("Task submitted,total tasks: " + std::to_string(getTotalTasksCount()));
            }
        }
        else
        {
            // 如果有返回值,返回future结果
            auto task = std::make_shared<std::packaged_task<return_type()>>([func = std::forward<F>(f), argu = std::make_tuple(std::forward<Args>(args)...)]() mutable
                                                                            { return std::apply(func, argu); }); // 使用指针确保生命周期

            std::future<return_type> resfuture = task->get_future();
            if (!taskqueue->push([task]()
                                 { (*task)(); }))
            {
                Logger::instance().error("Task submission failed!");
                std::promise<return_type> promise;
                promise.set_exception(std::make_exception_ptr(std::runtime_error("Task submit failed!\n")));
                return promise.get_future();
            }
            else
            {
                totalTasks.fetch_add(1);
                Logger::instance().debug("Task submitted,total tasks: " + std::to_string(getTotalTasksCount()));
                return resfuture;
            }
        }
    }

    // 状态查询
    size_t getThreadCount() const;         // 返回线程总数
    size_t getRunningThreadsCount() const; // 返回活跃线程
    size_t getTotalTasksCount() const;     // 返回总任务数
    size_t getCompletedCount() const;      // 返回完成任务数
    size_t getFailedTasksCount() const;    // 返回失败任务数
    size_t getTasksQueueSize() const;      // 返回任务队列大小
    bool isStopped() const;                // 返回是否停止
    std::string getStatus() const;         // 返回状态信息
};