#pragma once

#include <future>

/*
 * Wraps std::future reference to implement various await_* APIs.
 */
template <typename T>
class StdFutureWrapper {
public:
	StdFutureWrapper(std::future<T>& future) : future_(&future) {}

	bool await_ready() {
		return std::await_ready(*future_);
	}

	template <typename P>
	void await_suspend(std::experimental::coroutine_handle<P> awaiter) {
		return std::await_suspend(*future_, std::move(awaiter));
	}

	auto await_resume() {
		return std::await_resume(*future_);
	}

	StdFutureWrapper& get() {
		return *this;
	}

private:
	std::future<void>* future_{ nullptr };
};