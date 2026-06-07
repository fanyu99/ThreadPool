//对任务队列进行的封装
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
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
        if(shutdown.load())
            return false;
        task_queue.emplace(std::move(task));
        not_empty.notify_one();
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

// 多对多队列(双缓冲区: 生产队列-消费队列)
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