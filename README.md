# 高并发线程池

## 设计模式 "**单例模式**" 及其实现

```C++
//使用Meyers Singleton(C++11起线程安全,只在第一次调用时创建)
ThreadPool();
    ThreadPool(const ThreadPool &) = delete;//禁用拷贝构造
    ThreadPool &operator=(const ThreadPool &) = delete;//禁用=复制
    explicit ThreadPool(size_t threadnum);//禁止隐式的构造(例如:ThreadPool p=3;)
    ~ThreadPool();
    static ThreadPool &Pool(); // 只会在第一次调用时创建singleton
```

## 并发模型"**生产者-消费者模型**"

| 角色名称 |   对应实体    |
| :------: | :-----------: |
|  生产者  | 主线程/网络IO |
|  消费者  | 线程的Worker  |
|  缓冲区  |     queue     |
|   产品   |  执行的函数   |

### ***功能清单***

1. 线程管理:线程的创建销毁和回收(维护线程的容器)
2. 任务队列:存储待执行的任务的缓冲区(queue)     [^1]
3. 线程同步:保证线程安全(conditional_variable,mutex等)
4. 任务调度:提交任务进缓冲区(enqueue)
5. 生命周期:构造函数及析构函数

### ***代码实现***  [^2]

```C++
1. // 存储线程和维护线程的容器及其线程工作函数workerthread
    std::vector<std::thread> threadpool;       
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

2. //任务队列(缓冲区)
    std::unique_ptr<TaskQueue> taskqueue = std::make_unique<TaskQueue>();//任务队列基类指针
3. //线程同步:使用例如互斥锁,条件变量,原子变量确保线程安全
   //以下为具体例子:
    mutable std::mutex mtx;// mutble使其能在const函数中使用
    std::condition_variable cv;//条件变量(用于阻塞)
    std::atomic_bool stop;// 原子变量作为停止标志
4. //任务提交函数
    template <typename F, typename... Args>
    auto submit(F &&f, Args &&...args)
    {
        using return_type = std::invoke_result_t<F, Args...>;//推导返回值类型
        {
            std::mutex mtx;
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
            auto task = std::make_shared<std::packaged_task<return_type()>>([func = std::forward<F>(f), argu = std::make_tuple(std::forward<Args>(args)...)]() mutable                     { return std::apply(func, argu); }); // 使用指针确保生命周期

            std::future<return_type> resfuture = task->get_future();
            if (!taskqueue->push([task]()
                                    { (*task)(); }))
            {
                //如果提交失败,返回异常的future
                Logger::instance().error("Task submission failed!");
                std::promise<return_type> promise;
                promise.set_exception(std::make_exception_ptr(std::runtime_error("Task submit failed!\n")));
                return promise.get_future();
            }
            else
            {
                //如果提交成功,返回future对象
                totalTasks.fetch_add(1);
                Logger::instance().debug("Task submitted,total tasks: " + std::to_string(getTotalTasksCount()));
                return resfuture;
            }
        }
    }
5. //生命周期管理(构造函数及析构函数)
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

    //析构函数
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
```

[^1]: ***关于有锁队列和无锁队列***

无锁队列优势所在:避免线程阻塞和减少了系统调用:

1. 避免线程阻塞:
   无锁队列原子操作,无需阻塞和等待
2. 减少系统调用:
   1. 有锁队列在锁竞争时会需要系统调用进入内核态,线程调度器介入,上下文保存和恢复
   2. 无锁队列只需在用户态执行,避免以上开销
3. CPU利用率更高:
   1. 有锁队列锁竞争时CPU会空闲等待,缓存失效频繁
   2. 无锁队列CPU不会空闲等待,利用率更高,缓存亲和性更高
4. 无死锁和优先级反转问题:
    有锁队列可能出现死锁和优先级反转(低优先级线程持有锁,高优先级线程等待)

|   特性   |               有锁队列                |               无锁队列               |
| :------: | :-----------------------------------: | :----------------------------------: |
| 实现难度 |                 简单                  |                 复杂                 |
|   性能   |                 一般                  |             高并发下优秀             |
|   延迟   |               不可预测                |                可预测                |
| 内存使用 |                 较少                  |               可能较多               |
| 适用场景 |                 通用                  |        写多读少/低延迟/高并发        |
| 调试难度 |                 较易                  |                 困难                 |
| 工作流程 | a操作,b阻塞并等待,a完成,b被唤醒并操作 | a,b通过CAS操作,成功就继续,失败就重试 |

[^2]: ***具体需要的头文件及任务队列类的实现***

```C++
//1-头文件
#include <utility>  //通用工具(完美转发forward,移动语义move,)
#include <future>  //异步获取任务结果(future,packaged_task)
#include <thread>  //线程(thread)
#include <functional> //函数包装器(function)
#include <memory> //智能指针(unique_ptr,shrared_ptr)
#include <queue>  //任务队列(queue)
#include <mutex> //互斥锁(mutex)
#include <atomic> //原子操作(atomic)
#include <condition_variable>  //条件变量(condition_variable)
#include <vector>  //线程存储容器(vector)
#include <tuple>  //函数参数绑定(make_tuple,apply)
#include "../logger/include/logger.h" //日志类
#include "Task_Queue.h"               //封装的任务队列类
#include <stdexcept> //异常类(runtime_error)

//2-任务队列类的实现
// 基类抽象
enum class QueMode
{
    SPMC = 1, // 一对多
    MPMC = 2  // 多对多
};

class Task_Queue
{
    protected:
    std::atomic_bool shutdown;
public:
    explicit Task_Queue():shutdown{false} {}
    virtual ~Task_Queue()=default;                                 // 虚析构函数!!!
    virtual bool push(std::function<void()> task) = 0;     // 添加任务
    virtual bool pop(std::function<void()> &out_task) = 0; // 弹出任务(移动语义)
    virtual size_t size() const = 0;                       // 返回队列任务数
    virtual void Shutdown()=0;
};

// 一对多队列
class SPMC_Queue : public Task_Queue
{
private:
    std::queue<std::function<void()>> task_queue;
    std::condition_variable not_empty;
    mutable std::mutex mtx;

public:
    explicit SPMC_Queue() : Task_Queue() {}
    ~SPMC_Queue(){
        Shutdown();
    }
    bool push(std::function<void()> task) override
    {
        std::unique_lock<std::mutex> lock(mtx);
        if(shutdown.load()) // 关闭就退出
            return false;
        task_queue.emplace(std::move(task));
        not_empty.notify_one(); //唤醒
        return true;
    }
    bool pop(std::function<void()> &out_task) override
    {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this]()
                       { return !task_queue.empty()||shutdown.load(); });
        if(task_queue.empty()&&shutdown.load()) 
            return false;
        out_task = std::move(task_queue.front());
        task_queue.pop();
        return true;
    }
    size_t size()const {
        std::unique_lock<std::mutex> lock(mtx);
        return task_queue.size();
    }
    void Shutdown()override {
        shutdown.store(true);
        not_empty.notify_all();
    }
};

// 多对多队列
class MPMC_Queue : public Task_Queue
{
    private:
    std::condition_variable con_not_empty;            // 消费队列不空就不阻塞
    mutable std::mutex con_mtx; // 消费锁
    mutable std::mutex pro_mtx;
    std::queue<std::function<void()>> pro_task_queue; // 生产者队列
    std::queue<std::function<void()>> con_task_queue; // 消费者队列
public:
    MPMC_Queue():Task_Queue(){}
    ~MPMC_Queue(){
        Shutdown();
    };
    bool push(std::function<void()>task)override{
        std::unique_lock<std::mutex> lock(pro_mtx);
        if(shutdown.load())
            return false;
        pro_task_queue.emplace(std::move(task));
        con_not_empty.notify_one();
        return true;
    }
    bool pop(std::function<void()>&out_task)override {
        std::unique_lock<std::mutex>lock(con_mtx);
        con_not_empty.wait(lock, [this]()
        { 
            
           if(!con_task_queue.empty()||shutdown.load())return true;
           //消费队列为空,尝试去交换
           std::lock_guard<std::mutex> l(pro_mtx);
           if(!pro_task_queue.empty()){std::swap(pro_task_queue,con_task_queue);
               return true;
           }
           return false; // 两个都为空时阻塞
        
        });
        if(con_task_queue.empty())
            return false;
        out_task = std::move(con_task_queue.front());
        con_task_queue.pop();
        return true;
    }
    size_t size() const {
        std::unique_lock<std::mutex> lock(con_mtx);
        return con_task_queue.size()+pro_task_queue.size();
    }
    void Shutdown()override {
        shutdown.store(true);
        con_not_empty.notify_all();
    }
};
```
