#include "../include/threadpool.h"
// TODO:
// 构造函数
explicit ThreadPool::ThreadPool(size_t count) : stop{false}, threadcount{count == 0 ? std::thread::hardware_concurrency() : count}, totalTasks{0}, completeTasks{0}, failedTasks{0}, runningThreads{0}
{
    // 添加线程,每个线程都添加相应的工作函数
    for (size_t i = 0; i < threadcount; ++i)
    {
        threadpool.emplace_back([this]()
                                { this->workerThread(); });
    }
    Logger::instance().info("ThreadPool initialized! Initial threads: " + std::to_string(threadcount));
}
ThreadPool::ThreadPool() : stop{false}, threadcount{std::thread::hardware_concurrency()}, totalTasks{0}, completeTasks{0}, failedTasks{0}, runningThreads{0}
{
    for (size_t i = 0; i < threadcount; ++i)
    {
        threadpool.emplace_back([this]()
                                { this->workerThread(); });
    } 
    Logger::instance().info("ThreadPool initialized! Initial threads: " + std::to_string(threadcount));
}
// 单例模式
ThreadPool &ThreadPool::Pool(size_t threadcount)
{
    static ThreadPool pool(threadcount);
    return pool;
}
ThreadPool::~ThreadPool()
{
    Logger::instance().info("ThreadPool starting shutdown...");
    stop = true;
    cv.notify_all(); // 通知所有等待的线程
    // 等待所有线程完成任务
    for (auto &t : threadpool)
    {
        if (t.joinable())
            t.join();
    }
    Logger::instance().info("ThreadPool shutdown! The final status:\n" + this->getStatus());
}
// 线程工作函数
void ThreadPool::workerThread()
{
    std::ostringstream This_threadID;
    This_threadID << std::this_thread::get_id();
    Logger::instance().debug("Worker thread started, ID: " + This_threadID.str());
    while (true)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mtx); // 每个线程独立锁
            cv.wait(lock, [this]()
                    { return stop || !taskqueue.empty(); }); // 条件变量(条件谓词为false时)阻塞
            // 如果停止或者任务为空,停止
            if (stop && taskqueue.empty())
            {
                Logger::instance().debug("Worker thread exiting,ID: " + This_threadID.str());
                return;
            }
            // 取出任务执行
            task = std::move(taskqueue.front()); // 移动语义:队列任务只能转移,不可拷贝
            taskqueue.pop();
        }
        ++runningThreads;
        // 处理异常,防止单个任务出问题导致整个线程崩溃
        try
        {
            task(); // 执行任务
            ++completeTasks;
        }
        catch (const std::exception &e)
        {
            ++failedTasks;
            Logger::instance().error("Task error: " + std::string(e.what()));
        }
        catch (...)
        {
            ++failedTasks;
            Logger::instance().error("Unknown task error!");
        }
        --runningThreads;
    }
}
// 状态查询函数
size_t ThreadPool::getThreadCount() const
{ // 返回线程总数
    std::lock_guard<std::mutex> lock(mtx);
    return this->threadcount;
}
size_t ThreadPool::getRunningThreadsCount() const
{ // 返回活跃线程
    return this->runningThreads.load();
}
size_t ThreadPool::getTotalTasksCount() const
{ // 返回总任务数
    return this->totalTasks.load();
}
size_t ThreadPool::getCompletedCount() const
{
    // 返回完成任务数
    return this->completeTasks.load();
}
size_t ThreadPool::getFailedTasksCount() const
{
    // 返回失败任务数
    return this->failedTasks.load();
}
size_t ThreadPool::getTasksQueueSize() const
{
    // 返回任务队列大小(共享变量,需要加锁保护)
    std::lock_guard<std::mutex> lock(mtx); //
    return this->taskqueue.size();
}
bool ThreadPool::isStopped() const
{ // 返回是否停止
    return this->stop.load();
}
std::string ThreadPool::getStatus() const
{ // 返回详细的格式化状态信息
    std::ostringstream oss;
    oss << "===ThreadPool Status===\n";
    oss << "  Thread Count: " << getThreadCount() << "\n";
    oss << "  Active Threads: " << runningThreads.load() << "\n";
    oss << "  Total Tasks: " << totalTasks.load() << "\n";
    oss << "  Completed Tasks: " << completeTasks.load() << "\n";
    oss << "  Failed Tasks: " << failedTasks.load() << "\n";
    oss << "  Queue Size: " << getTasksQueueSize() << "\n";
    oss << "  Stopped: " << (stop.load() ? "Yes" : "No") << "\n";
    if (totalTasks.load() > 0)
    {
        double successRate = static_cast<double>(completeTasks.load()) / totalTasks.load() * 100.0;
        oss << "  Success Rate: " << std::fixed << std::setprecision(2) << successRate << "%\n";
    }
    oss << "===Status Above===";
    return oss.str();
}