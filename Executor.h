#pragma once

#include <functional>

/*
 * This is a primitive Executor API. It's not neccesarily what we want to keep long
 * term, but good enough for this example.
 */
class Executor {
public:
	virtual void add(std::function<void()> func) = 0;

	// for testing purposes only
	virtual bool isInExecutor() = 0;

	virtual ~Executor() {}
};
