//对任务队列进行的封装

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <vector>
#include <type_traits>



// 队列模式
enum class QueueMode
{
	SPMC, // 一对多
	MPMC    // 多对多
};

// 抽象类
class Task_Queue
{
protected:
	std::atomic_bool shutdown{false};
public:
	explicit Task_Queue() = default;
	virtual ~Task_Queue() = default;                                 // 虚析构函数!!!
	virtual bool push( std::function<void()> task )=0;     // 添加任务
	virtual bool pop( std::function<void()>& out_task )=0; // 弹出任务(移动语义)

	virtual size_t size() const = 0;                       // 返回队列任务数
	virtual void Shutdown() = 0;
};

// 一对多队列
class SPMC_Queue : public Task_Queue
{
private:
	std::queue<std::function<void()>> task_queue;
	std::condition_variable not_empty;
	mutable std::mutex mtx;

public:
	explicit SPMC_Queue();
	~SPMC_Queue();
	// 阻塞式入队
	bool push( std::function<void()> task ) override;
	bool pop( std::function<void()>& out_task ) override;
	size_t size()const;
	void Shutdown()override;
};

// 多对多队列(双缓冲区: 生产队列 - 消费队列)
class MPMC_Queue : public Task_Queue
{
private:
	std::condition_variable con_not_empty;            // 消费队列不空就不阻塞
	mutable std::mutex con_mtx; // 消费锁
	mutable std::mutex pro_mtx;	// 生产锁
	std::queue<std::function<void()>> pro_task_queue; // 生产者队列
	std::queue<std::function<void()>> con_task_queue; // 消费者队列
public:
	MPMC_Queue();
	~MPMC_Queue();
	bool push( std::function<void()>task )override;
	bool pop( std::function<void()>& out_task )override;
	size_t size() const;
	void Shutdown()override;
};


// 默认的缓冲容量
namespace BoundedQueueConfig
{
	constexpr size_t DEFAULT_CAPACITY = 1024;

}
// 高频采集的无锁有界循环缓冲（大小为 2 的幂），适用于SPSC模式
// 背压策略直接拒绝写入 
template <typename T, size_t Capacity = BoundedQueueConfig::DEFAULT_CAPACITY>
class BoundedQueue {
	// 编译器校验是否合法
	static_assert((Capacity& (Capacity - 1)) == 0, "缓冲区大小必须是2的幂");
	static_assert(std::is_trivially_copyable_v<T>, "T 必须是可平凡复制的类型");

public:
	// 使用(current+1)  & （Capacity - 1） 来索引回绕 ，提升效率
	static constexpr size_t capacity_{ Capacity };	 // 缓冲区固定容量

	// 生产者尝试入队，写入为覆盖式
	// 停止时，不入队直接返回false
	bool try_push( const T& in_data ) {
		// 正在停止，直接返回false
		if (is_stopping()) {
			return false;
		}

		size_t w = write_index.load( std::memory_order_relaxed );		// 读取自己（生产者）的索引，只需要relaxed
		size_t r = read_index.load( std::memory_order_acquire );	  // 读取消费者的索引，保证看到消费者释放的位置

		//  如果缓冲区满，直接返回，上层需要进行背压
		if (w - r == Capacity) {
			return false;
		}

		// 更新数据
		buffer_[w & (Capacity - 1)] = in_data;

		write_index.store( w + 1, std::memory_order_release ); // 更新自己（生产者）索引，保证数据写入完成后才更新索引

		return true;
	}

	// 消费者尝试出队，需要判断是否有数据/是否正在写入
	bool try_pop( T& out_data ) {
		size_t r = read_index.load( std::memory_order_relaxed );		// 读取自己（消费者）的索引，只需要relaxed
		size_t w = write_index.load( std::memory_order_acquire );	  // 读取生产者的索引，保证看到生产者写入的数据

		// 如果缓冲区空，返回
		if (r == w) {
			return false;
		}

		out_data = std::move( buffer_[r & (Capacity - 1)] ); // 读取数据
		read_index.store( r + 1, std::memory_order_release ); // 更新自己（消费者）索引，保证数据读取完成后才更新索引
		return true;
	}

	// 停止队列
	void shutdown() {
		stop_.store( true, std::memory_order_release );
	}

	bool is_stopping() const {
		return stop_.load( std::memory_order_acquire );
	}

	// 获取当前的元素个数
	size_t size() const {
		size_t w = write_index.load( std::memory_order_acquire );
		size_t r = read_index.load( std::memory_order_acquire );
		return w - r;
	}

	BoundedQueue() {

	}
	~BoundedQueue() {
		shutdown();
	}

private:
	// 缓冲区
	std::vector<T> buffer_ = std::vector<T>( Capacity ); // 循环缓冲区（注意不能写 buffer_{Capacity}，那会创建 1 个元素）
	std::atomic<bool> stop_{ false }; // 停止标志位

	// 生产者
	alignas(64) std::atomic<size_t > write_index{ 0 };	 // 生产者在头部写入

	// 消费者
	alignas(64) std::atomic<size_t > read_index{ 0 };	// 消费者在尾部读取

};