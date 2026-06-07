#include <thread>
#include <tuple>
#include <iostream>
#include <queue>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
class Task_Queue{
    std::queue<std::function<void>> task_queue;//任务队列
    std::mutex mtx;//锁
    public:
    template <typename F ,typename ...Args >     
    bool try_enqueue(F&&f,Args&& ...args) {
        auto task = [func = std::forward<F>(f), ags = std::make_tuple(std::forward<Args>(args)...)]() mutable
        { std::apply(func, ags); };
        {
            std::lock_guard<std::mutex> lock(mtx);
            task_queue.emplace(std::move(task));
        }
    };
};
class ThreadPool
{
};