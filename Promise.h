#pragma once

#include "Allocator.h"
#include "AwaitWrapper.h"
#include "ExecutionContext.h"
#include "Task.h"
#include "StdFutureWrapper.h"

enum class PromiseState {
	// Coroutine hasn't started
	EMPTY,
	// Coroutine is running, but Future object managing this coroutine was destroyed
	DETACHED,
	// Some other coroutine is waiting on this coroutine to be complete
	HAS_AWAITER,
	// Coroutine is finished, result is stored inside Promise
	HAS_RESULT
};

template <typename T> 
class Future;

template <typename T>
class Promise {
public:
	using State = PromiseState;

	template <typename... Args>
	void* operator new(size_t size, AllocatorPtr allocator, Args&&...) {
		size += sizeof(Allocator*);
		Allocator** ptr;
		if (allocator) {
			ptr = static_cast<Allocator**>(allocator->allocate(size));
		}
		else {
			ptr = static_cast<Allocator**>(malloc(size));
		}
		*ptr = allocator;
		return ptr + 1;
	}

	template <typename... Args>
	void* operator new(size_t size, Args&&...) {
		static_assert(false, "AllocatorPtr should be the first argument for every Task<T>.");
	}

	void operator delete(void* ptr, size_t size) {
		auto allocator = *(static_cast<Allocator**>(ptr) - 1);
		ptr = static_cast<Allocator**>(ptr) - 1;

		if (allocator) {
			allocator->deallocate(ptr, size + sizeof(Allocator*));
		}
		else {
			free(ptr);
		}
	}

	Promise() {
	}

	~Promise() {
	}

	Task<T> get_return_object() {
		return{ *this };
	}

	std::experimental::suspend_always initial_suspend() {
		return{};
	}

	template <typename... Args>
	auto await_transform(CallableTask<Args...>&& CallableTask) {
		auto task = CallableTask.init(executionContext_.allocator);
		auto future = std::move(task).startInline(executionContext_);
		return future;
	}

	template <typename U>
	auto await_transform(std::future<U>& future) {
		return AwaitWrapper<StdFutureWrapper<U>>::create({ future }, executionContext_.executor);
	}

	template <typename U>
	AwaitWrapper<std::reference_wrapper<Future<U>>> await_transform(Future<U>& future) {
		assert(executionContext_.executor->isInExecutor());

		if (future.promise_->executionContext_.executor ==
			executionContext_.executor) {

			return{ std::ref(future) };
		}

		return AwaitWrapper<std::reference_wrapper<Future<U>>>::create(std::ref(future), executionContext_.executor);
	}

	class FinalSuspender;

	FinalSuspender final_suspend() {
		return{ *this };
	}

	template <typename U>
	void return_value(U&& value) {
		assert(executionContext_.executor->isInExecutor());
		resultValue_ = std::make_unique<T>(std::forward<U>(value));
	}

	void set_exception(std::exception_ptr exception) {
		assert(executionContext_.executor->isInExecutor());
		resultException_ = std::move(exception);
	}

	void start() {
		assert(executionContext_.executor->isInExecutor());
		std::experimental::coroutine_handle<Promise>::from_promise(*this)();
	}

private:
	friend class Future<T>;
	friend class Task<T>;

	std::atomic<State> state_{ State::EMPTY };

	// Should be some variant type (e.g. folly::Try) instead
	std::unique_ptr<T> resultValue_;
	std::exception_ptr resultException_;

	std::experimental::coroutine_handle<> awaiter_;

	ExecutionContext executionContext_;
};

template <typename T>
class Promise<T>::FinalSuspender {
public:
	bool await_ready() {
		return promise_.state_.load(std::memory_order_acquire) == State::DETACHED;
	}

	bool await_suspend(std::experimental::coroutine_handle<>) {
		auto state = promise_.state_.load(std::memory_order_acquire);

		do {
			if (state == State::DETACHED) {
				return false;
			}
			assert(state != State::HAS_RESULT);
		} while (!promise_.state_.compare_exchange_weak(
			state,
			State::HAS_RESULT,
			std::memory_order_release,
			std::memory_order_acquire));

		if (state == State::HAS_AWAITER) {
			promise_.awaiter_.resume();
		}

		return true;
	}

	void await_resume() {}

private:
	friend class Promise;

	FinalSuspender(Promise& promise) : promise_(promise) {}
	Promise& promise_;
};