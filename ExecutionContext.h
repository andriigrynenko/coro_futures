#pragma once

class Executor;
class Allocator;

struct ExecutionContext {
	Executor* executor{ nullptr };
	Allocator* allocator{ nullptr };
};
