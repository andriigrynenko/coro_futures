#pragma once

class Allocator {
public:
	virtual void* allocate(size_t size) = 0;
	virtual void deallocate(void* buffer, size_t size) = 0;
	virtual ~Allocator() {}
};

using AllocatorPtr = Allocator*;