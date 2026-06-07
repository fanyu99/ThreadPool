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
    // 线程工作函数workerThread
    void ThreadPool::workerThread()
    {
        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx); // 每个线程独立锁
                cv.wait(lock, [this]()
                        { return stop || !taskqueue.empty(); }); // 条件变量(条件谓词为false,即线程池未销毁但缓冲区已为空时)阻塞
                // 如果线程池销毁且任务为空,停止工作
                if (stop && taskqueue.empty())
                {
                    return;
                }
                // 取出任务执行
                task = std::move(taskqueue.front()); // 移动语义:队列任务只能转移,不可拷贝
                taskqueue.pop();
            }
            //原子变量无需使用互斥锁(可提高效率)
            ++runningThreads;
            // 处理异常,防止单个任务出问题导致整个线程崩溃
            try
            {
                task(); // 执行任务
                ++completeTasks;//完成的任务数
            }
            catch (const std::exception &e)
            {
                ++failedTasks;//未完成的任务数
            }
            catch (...)
            {
                ++failedTasks;
            }
            --runningThreads;//正在工作的线程
        }
    }
2. //任务队列(缓冲区)
    std::queue<std::function<void()>> taskqueue;//创建function函数包装器(进行类型擦除,比模板队列更通用,但是会带来轻微性能开销)的队列
3. //线程同步:使用例如互斥锁,条件变量,原子变量确保线程安全
   //以下为具体例子:
    mutable std::mutex mtx;// mutble使其能在const函数中使用
    std::condition_variable cv;//条件变量(用于阻塞)
    std::atomic_bool stop;// 原子变量作为停止标志
4. //任务调度enqueue函数
    // 任务入列
    template <typename F, typename... Args>
    void entaskque(F &&f, Args &&...args)
    {
        ++totalTasks;//总任务数
        auto task = [func = std::forward<F>(f), argu = std::make_tuple(std::forward<Args>(args)...)]() mutable
        { std::apply(func, argu); };// 使用元组进行参数绑定,使用apply执行
        {
            std::unique_lock<std::mutex> lock(mtx);//互斥锁保证线程安全
            //如果线程池停止工作,中断入列
            if (stop)
            {
                throw std::runtime_error("ThreadPool is stopped!\n");
            }
            taskqueue.emplace(std::move(task)); // 任务入列,lambda不可拷贝,使用移动语义
        }
        cv.notify_one(); // 唤醒一个进程
    }
5. //生命周期管理(构造函数及析构函数)
    // 构造函数
    ThreadPool::ThreadPool(size_t count) : stop{false}, threadcount{count == 0 ? std::thread::hardware_concurrency() : count}, totalTasks{0}, completeTasks{0}, failedTasks{0}, runningThreads{0}
    {
        // 添加线程,每个线程都添加相应的工作函数
        for (size_t i = 0; i < threadcount; ++i)
        {
            threadpool.emplace_back([this]()
                                    { this->workerThread(); });
        }
    }
    ThreadPool::ThreadPool() : stop{false}, threadcount{std::thread::hardware_concurrency()}, totalTasks{0}, completeTasks{0}, failedTasks{0}, runningThreads{0}
    {
        for (size_t i = 0; i < threadcount; ++i)
        {
            threadpool.emplace_back([this]()
                                    { this->workerThread(); });
        }
    }
    //析构函数
    ThreadPool::~ThreadPool()
    {
        stop = true;
        cv.notify_all(); // 唤醒所有阻塞的线程
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

[^2]: ***具体需要的头文件及函数类型不同而分化的提交任务的函数实现***

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

//2-任务提交函数
// 无返回值函数任务的提交
template <typename F, typename... Args>
void submit(F &&f, Args &&...args)
{
    this->entaskque(f, args...);
    Logger::instance().debug("Task submitted,total tasks: " + std::to_string(getTotalTasksCount()));
}
// 有返回值函数任务的提交(返回future异步获取结果)
template <typename F, typename... Args>
auto submit_result(F &&f, Args &&...args)  //返回std::future对象用于异步获取结果
{
    ++totalTasks;
    using return_type = decltype(f(args...)); // 推导返回值类型
    // 使用lambda函数,使用tuple绑定参数,apply执行包装函数
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [func = std::forward<F>(f), arg = std::make_tuple(std::forward<Args>(args)...)]() mutable
        { return std::apply(func, arg); });               // 使用智能指针确保lambda执行时仍然有效
    std::future<return_type> result = task->get_future(); // 异步获取结果
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
    return result; // 返回future对象,后续异步获取结果
}
```
