#pragma once

#include "Executor.h"

template <typename FutureRef>
class AwaitWrapper {
public:
	struct promise_type {
		std::experimental::suspend_always initial_suspend() { return{}; }

		std::experimental::suspend_never final_suspend() {
			executor_->add([awaiter = std::move(awaiter_)]() mutable {
				awaiter();
			});
			return{};
		}

		void return_void() {}

		AwaitWrapper get_return_object() {
			return{ *this };
		}

		Executor* executor_;
		std::experimental::coroutine_handle<> awaiter_;
	};

	AwaitWrapper(FutureRef futureRef) : futureRef_(std::move(futureRef)) {}

	static AwaitWrapper<FutureRef> awaitWrapper() {
		co_return;
	}

	static AwaitWrapper create(FutureRef futureRef, Executor* executor) {
		auto ret = awaitWrapper();
		new(&ret.futureRef_)FutureRef(std::move(futureRef));
		ret.promise_->executor_ = executor;
		return ret;
	}

	bool await_ready() {
		return futureRef_.get().await_ready();
	}

	static constexpr bool await_suspend_returns_void =
		std::is_same<decltype((*static_cast<FutureRef*>(nullptr)).get().await_suspend(std::experimental::coroutine_handle<>())), void >::value;

	template <typename _ = std::enable_if_t<!await_suspend_returns_void>,
		typename __ = void>
		bool await_suspend(std::experimental::coroutine_handle<> awaiter) {
		if (promise_) {
			promise_->awaiter_ = std::move(awaiter);
			return futureRef_.get().await_suspend(
				std::experimental::coroutine_handle<promise_type>::from_promise(*promise_));
		}

		return futureRef_.get().await_suspend(awaiter);
	}

	template <typename _ = std::enable_if_t<await_suspend_returns_void>>
	void await_suspend(std::experimental::coroutine_handle<> awaiter) {
		if (promise_) {
			promise_->awaiter_ = std::move(awaiter);
			futureRef_.get().await_suspend(
				std::experimental::coroutine_handle<promise_type>::from_promise(*promise_));
			return;
		}

		futureRef_.get().await_suspend(awaiter);
	}

	decltype((*static_cast<FutureRef*>(nullptr)).get().await_resume()) await_resume() {
		return futureRef_.get().await_resume();
	}

private:
	AwaitWrapper(promise_type& promise) : promise_(&promise) {}

	promise_type* promise_{ nullptr };
	// Should be std::optional, we don't call it's destructor, but that should be ok for ref types
	union {
		FutureRef futureRef_;
	};
};