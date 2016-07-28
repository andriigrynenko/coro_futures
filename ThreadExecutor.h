#pragma once

#include <future>
#include <queue>

#include "Executor.h"

/*
 * Simple single thread executor. Manages its own thread and executes all scheduled tasks 
 * on that thread in FIFO order.
 */
class ThreadExecutor : public Executor {
public:
	ThreadExecutor() {
		std::promise<std::thread::id> threadIdPromise;
		auto threadIdFuture = threadIdPromise.get_future();
		thread_ = std::thread([&, threadIdPromise = std::move(threadIdPromise)]() mutable {
			threadIdPromise.set_value(std::this_thread::get_id());
			while (true) {
				std::unique_lock<std::mutex> lock(queueMutex_);

				while (!stop_ && queue_.empty()) {
					queueCV_.wait(lock);
				}

				while (!queue_.empty()) {
					queue_.front()();
					queue_.pop();
				}

				if (stop_) {
					break;
				}
			}
		});
		threadId_ = threadIdFuture.get();
	}

	virtual void add(std::function<void()> func) {
		std::lock_guard<std::mutex> lg(queueMutex_);

		if (stop_) {
			throw std::logic_error("Adding a task after thread was stopped");
		}

		queue_.push(std::move(func));
		queueCV_.notify_all();
	}

	virtual bool isInExecutor() {
		return threadId_ == std::this_thread::get_id();
	}

	void join() {
		{
			std::lock_guard<std::mutex> lg(queueMutex_);
			stop_ = true;
			queueCV_.notify_all();
		}

		thread_.join();
	}

private:
	bool stop_{ false };
	std::mutex queueMutex_;
	std::condition_variable queueCV_;
	std::queue<std::function<void()>> queue_;

	std::thread::id threadId_;
	std::thread thread_;
};