#pragma once

#include "Executor.h"
#include "Promise.h"
#include "Task.h"
#include "StackAllocator.h"

/*
 * Future object attached to a running coroutine. Implement await_* APIs.
 */
template <typename T>
class Future {
public:
	Future(const Future&) = delete;
	Future(Future&& other) : promise_(other.promise_) {
		other.promise_ = nullptr;
	}

	void wait() {
		prep(*this).get();
	}

	T& get() {
		assert(promise_->state_ == Promise<T>::State::HAS_RESULT);
		assert(promise_->state_ == Promise<T>::State::HAS_RESULT);
		if (promise_->resultValue_) {
			return *promise_->resultValue_;
		}
		std::rethrow_exception(promise_->resultException_);
	}

	T& operator*() {
		return get();
	}

	T* operator->() {
		return &get();
	}

	bool await_ready() {
		return promise_->state_.load(std::memory_order_acquire) == Promise<T>::State::HAS_RESULT;
	}

	bool await_suspend(std::experimental::coroutine_handle<> awaiter) {
		auto state = promise_->state_.load(std::memory_order_acquire);

		if (state == Promise<T>::State::HAS_RESULT) {
			return false;
		}
		assert(state == Promise<T>::State::EMPTY);

		promise_->awaiter_ = std::move(awaiter);

		if (promise_->state_.compare_exchange_strong(
			state,
			Promise<T>::State::HAS_AWAITER,
			std::memory_order_release,
			std::memory_order_acquire)) {

			return true;
		}

		assert(promise_->state_ == Promise<T>::State::HAS_RESULT);
		return false;
	}

	T await_resume() { return std::move(get()); }

	~Future() {
		if (!promise_) {
			return;
		}

		auto state = promise_->state_.load(std::memory_order_acquire);

		do {
			assert(state != Promise<T>::State::DETACHED);
			assert(state != Promise<T>::State::HAS_AWAITER);

			if (state == Promise<T>::State::HAS_RESULT) {
				auto ch = std::experimental::coroutine_handle<Promise<T>>::from_promise(*promise_);
				assert(ch.done());
				ch.destroy();
				return;
			}
			assert(state == Promise<T>::State::EMPTY);
		} while (!promise_->state_.compare_exchange_weak(
			state,
			Promise<T>::State::DETACHED,
			std::memory_order::memory_order_release,
			std::memory_order::memory_order_acquire));
	}

private:
	friend class Task<T>;

	std::future<void> prep(Future& future) {
		co_await future;
		co_return;
	};

	Future(Promise<T>& promise) : promise_(&promise) {

	}

	Promise<T>* promise_;
};

/*
 * Start coroutine on a given executor, using fixed size stack allocator.
 */
template <typename F, typename... Args>
auto spawn(Executor& executor, size_t stackSize, F&& f, Args&&... args) {
	ExecutionContext ec;
	ec.executor = &executor;
	ec.allocator = StackAllocator::create(stackSize);
	auto task = f(ec, std::forward<Args>(args)...);
	auto future = std::move(task).start(ec);
	return future;
}

/*
 * Start coroutine on a given executor. Malloc allocator is used.
 */
template <typename T>
auto spawn(Executor& executor, Task<T>&& task) {
	ExecutionContext ec;
	ec.executor = &executor;
	auto future = task.start(ec);
	return std::move(future);
}

/*
 * Schedule coroutine to be started inline using current executor and allocator.
 * The coroutine doesn't start until the return object of call(...) is awaited on.
 */
template <typename F, typename... Args>
auto call(F&& f, Args&&... args) {
	return CallableTask<F, Args...>(std::forward<F>(f), std::forward<Args>(args)...);
}
