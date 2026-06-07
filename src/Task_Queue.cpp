#include "Task_Queue.h"
#include <atomic>

// 一对多队列实现
SPMC_Queue::SPMC_Queue() : Task_Queue() {}
SPMC_Queue::~SPMC_Queue() {
	Shutdown();
}
// 阻塞式入队
bool SPMC_Queue::push( std::function<void()> task ) {
	std::unique_lock<std::mutex> lock( mtx );  // 加锁
	if (shutdown.load( std::memory_order_acquire ))
		return false;
	task_queue.emplace( std::move( task ) ); // 入队;
	not_empty.notify_one();			   // 唤醒一个等待线程
	return true;
}
bool SPMC_Queue::pop( std::function<void()>& out_task ) {
	std::unique_lock<std::mutex> lock( mtx );
	// 条件变量阻塞直到队列不为空或者关闭
	not_empty.wait( lock, [ this ]()
		{ return !task_queue.empty() || shutdown.load(); } );
	// 如果关闭，且队列空，退出
	if (task_queue.empty() && shutdown.load( std::memory_order_acquire ))
		return false;
	out_task = std::move( task_queue.front() );
	task_queue.pop();
	return true;
}

size_t SPMC_Queue::size() const {
	std::unique_lock<std::mutex> lock( mtx );
	return task_queue.size();
}
void SPMC_Queue::Shutdown() {
	shutdown.store( true, std::memory_order_release );
	not_empty.notify_all(); // 唤醒所有等待线程
}


// 多对多队列实现
MPMC_Queue::MPMC_Queue() : Task_Queue() {}

MPMC_Queue::~MPMC_Queue() {
	Shutdown();
}

bool MPMC_Queue::push( std::function<void()> task ) {
	// 如果正在关闭，不入队
	if (shutdown.load( std::memory_order_acquire ))
		return false;
	std::lock_guard<std::mutex> lock( pro_mtx );
	pro_task_queue.emplace( std::move( task ) ); // 入队
	con_not_empty.notify_one();				   // 唤醒一个等待线程
	return true;
}

bool MPMC_Queue::pop( std::function<void()>& out_task ) {
	std::unique_lock<std::mutex> lock( con_mtx );
	con_not_empty.wait( lock, [ this ]()
		{
			// 如果消费队列不空或者关闭,就不阻塞
			if (!con_task_queue.empty() || shutdown.load( std::memory_order_acquire ))
				return true;
			// 生产队列有数据，但是消费队列为空,尝试去交换
			std::lock_guard<std::mutex> l( pro_mtx );
			if (!pro_task_queue.empty()) {
				std::swap( pro_task_queue, con_task_queue );
				return true;
			}
			return false; // 两个都为空时阻塞
		} );
	// 如果队列都为空且关闭，就退出
	if (con_task_queue.empty() && shutdown.load( std::memory_order_acquire ))
		return false;
	out_task = std::move( con_task_queue.front() );
	con_task_queue.pop();
	return true;
}

size_t MPMC_Queue::size() const {
	std::unique_lock<std::mutex> lock( con_mtx );
	std::lock_guard<std::mutex> l( pro_mtx );
	return con_task_queue.size() + pro_task_queue.size();
}
			
void MPMC_Queue::Shutdown() {
	shutdown.store( true, std::memory_order_release );
	con_not_empty.notify_all();
}


