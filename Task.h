#pragma once

#include "ExecutionContext.h"

template <typename T>
class Promise;

template <typename T>
class Future;

/*
 * Represents allocated, but not-started coroutine, which is not yet assigned to any executor.
 */
template <typename T>
class Task {
public:
	using promise_type = Promise<T>;

	Task(const Task&) = delete;
	Task(Task&& other) : promise_(other.promise_) {
		other.promise_ = nullptr;
	}

	~Task() {
		assert(!promise_);
	}

	Future<T> startInline(ExecutionContext ec) && {
		assert(ec.executor->isInExecutor());
		promise_->executionContext_ = ec;
		promise_->start();
		return{ *std::exchange(promise_, nullptr) };
	}

	Future<T> start(ExecutionContext ec) && {
		promise_->executionContext_ = ec;
		ec.executor->add([promise = promise_] {
			promise->start();
		});
		return{ *std::exchange(promise_, nullptr) };
	}

private:
	friend class promise_type;

	Task(promise_type& promise) : promise_(&promise) {

	}

	Promise<T>* promise_;
};

/*
 * Represents non-allocated coroutine, stores references to all coroutine arguments.
 */
template <typename F, typename... Args>
class CallableTask {
public:
	CallableTask(F&& f, Args&&... args) :
		f_(BindRef()(std::forward<F>(f), std::forward<Args>(args)...)) {}

	auto init(AllocatorPtr allocator) {
		return f_(allocator);
	}

private:
	struct BindRef {
		auto operator()(F&& f, Args&&... args) {
			return [&](AllocatorPtr allocator) mutable {
				return f(allocator, std::forward<Args>(args)...);
			};
		}
	};

	using Func = typename std::result_of<BindRef(F&&, Args&&...)>::type;
	Func f_;
};