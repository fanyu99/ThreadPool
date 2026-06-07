#include "../include/threadpool.h"

// 构造函数
ThreadPool::ThreadPool(size_t count, QueMode mode) : stop{false}, threadcount{count == 0 ? std::thread::hardware_concurrency() : count}, totalTasks{0}, completeTasks{0}, failedTasks{0}, runningThreads{0}, quemode{mode}
{
    switch (quemode)
    {
    case QueMode::SPMC:
        taskqueue = std::make_unique<SPMC_Queue>();
        Logger::instance().debug("task_queue mode: SPMC");
        break;
    case QueMode::MPMC:
        taskqueue = std::make_unique<MPMC_Queue>();
        Logger::instance().debug("task_queue mode: MPMC");
        break;
    default:
        Logger::instance().error("invalid argument for task_queue mode!");
        break;
    }
    // 添加线程,每个线程都添加相应的工作函数
    for (size_t i = 0; i < threadcount; ++i)
    {
        threadpool.emplace_back([this]()
                                { this->workerThread(); });
    }
    Logger::instance().info("ThreadPool initialized! Initial threads: " + std::to_string(threadcount));
}

ThreadPool::ThreadPool(QueMode mode) : stop{false}, threadcount{std::thread::hardware_concurrency()}, totalTasks{0}, completeTasks{0}, failedTasks{0}, runningThreads{0}, quemode{mode}
{
    switch (quemode)
    {
    case QueMode::SPMC:
        taskqueue = std::make_unique<SPMC_Queue>();
        Logger::instance().debug("task_queue mode: SPMC");
        break;
    case QueMode::MPMC:
        taskqueue = std::make_unique<MPMC_Queue>();
        Logger::instance().debug("task_queue mode: MPMC");
        break;
    default:
        Logger::instance().error("invalid argument for task_queue mode!");
        break;
    }
    for (size_t i = 0; i < threadcount; ++i)
    {
        threadpool.emplace_back([this]()
                                { this->workerThread(); });
    }
    Logger::instance().info("ThreadPool initialized! Initial threads: " + std::to_string(threadcount));
}


// 单例模式
ThreadPool &ThreadPool::Pool(size_t threadcount, QueMode mode)
{
    static ThreadPool pool(threadcount, mode);
    return pool;
}
ThreadPool::~ThreadPool()
{
    Logger::instance().info("ThreadPool starting shutdown...");
    stop.store(true);
    taskqueue->Shutdown(); //关闭任务队列
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
        if(!taskqueue->pop(task)){
            Logger::instance().debug("Worker thread exiting or queue is empty,ID: " + This_threadID.str());
            break;
        }
        runningThreads.fetch_add(1);
        try{
            task();
            completeTasks.fetch_add(1);
        }catch(...){
            failedTasks.fetch_add(1);
            Logger::instance().warning("one task failed in thread,ID: " + This_threadID.str());
        }
        runningThreads.fetch_sub(1);
    }
}


// 状态查询函数
size_t ThreadPool::getThreadCount() const
{ // 返回线程总数
    return this->threadcount.load();
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
    return taskqueue->size();   
}
bool ThreadPool::isStopped() const
{ // 返回是否停止
    return this->stop.load();
}
std::string ThreadPool::getStatus() const
{ // 返回状态信息
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