#pragma once
#ifndef CONCURRENTQUEUE_H
#define CONCURRENTQUEUE_H

#include <mutex>
#include <condition_variable>
#include <deque>

/**************************************************************/
/* This class wraps a concurrency mutex around an std::deque  */
/* It blocks the thread that wants to pop() until there's     */
/* something in the queue.                                    */
/**************************************************************/
template <typename T>
class ConcurrentQueue
{
private:
	std::mutex              d_mutex;
	std::condition_variable d_condition;
	std::deque<T>           d_queue;
	size_t					d_max_size = 0;
public:
	// call push() using std::move
	void push(T&& value) {
		{
			std::unique_lock<std::mutex> lock(this->d_mutex);
			this->d_queue.push_front(std::move(value));
		}
		if (this->d_queue.size() > this->d_max_size)
			this->d_max_size = this->d_queue.size();
		this->d_condition.notify_one();
	}
	T pop() {
		std::unique_lock<std::mutex> lock(this->d_mutex);
		this->d_condition.wait(lock, [this] { return !this->d_queue.empty(); });
		T rc(std::move(this->d_queue.back()));
		this->d_queue.pop_back();
		return rc;
	}
	void resize(int size) {
		this->d_queue.resize(size);
	}
	void clear() {
		std::unique_lock<std::mutex> lock(this->d_mutex);
		this->d_queue.clear();
		this->d_max_size = 0;
	}
	int size() {
		return this->d_queue.size();
	}
	size_t max_size() {
		return this->d_max_size;
	}
};

#endif // CONCURRENTQUEUE_H
