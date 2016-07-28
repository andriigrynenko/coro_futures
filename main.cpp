#include <iostream>
#include <future>
#include <experimental/generator>
#include <cassert>
#include <functional>
#include <queue>
#include <type_traits>

#include "ThreadExecutor.h"
#include "Future.h"

void printThreadID() {
	std::cout << "Current thread ID = " << std::this_thread::get_id() << std::endl;
}

Task<int> test2(ExecutionContext, std::future<void> baton) {
	std::cout << "test2()" << std::endl;
	printThreadID();

	co_await baton;

	std::cout << "test2(): done awaiting baton" << std::endl;
	printThreadID();

	co_return 24;
}

Task<int> test(ExecutionContext, std::future<void> baton) {
	std::cout << "test()" << std::endl;
	printThreadID();

	auto value = co_await call(test2, std::move(baton));
	assert(value == 24);

	std::cout << "test(): done awaiting test2" << std::endl;
	printThreadID();

	co_return 42 + value;
}

int main() {
	ThreadExecutor executor;

	std::promise<void> baton;

	std::cout << "main(): spawning test()" << std::endl;
	printThreadID();
	auto future = spawn(executor, 1024, test, baton.get_future());

	std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });

	std::cout << "main(): posting baton" << std::endl;
	printThreadID();

	baton.set_value();

	future.wait();

	assert(future.await_ready());
	assert(*future == 66);

	std::cout << "main(): done awaiting test()" << std::endl;
	printThreadID();

	executor.join();

	return 0;
}