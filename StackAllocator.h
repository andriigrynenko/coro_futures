#pragma once

#include <cassert>

#include "Allocator.h"

/*
 * Fixed size stack allocator. Memory should be deallocated LIFO order.
 * Destroys itself once the first allocated segment is deallocated.
 */
class StackAllocator : public Allocator {
public:
	void* allocate(size_t size) {
		if (ptr_ - size <= buffer_ - size_) {
			return nullptr;
		}
		return ptr_ -= size;
	}

	void deallocate(void* ptr, size_t size) {
		assert(ptr == ptr_);
		ptr_ += size;

		if (ptr_ == buffer_) {
			delete this;
		}
	}

	static StackAllocator* create(size_t size) {
		auto ptr = static_cast<StackAllocator*>(malloc(sizeof(StackAllocator) + size));
		new (ptr)StackAllocator(size, ptr + 1);
		return ptr;
	}

	StackAllocator(const StackAllocator&) = delete;
	StackAllocator& operator=(const StackAllocator&) = delete;

private:
	StackAllocator(size_t size, void* buffer)
		: buffer_(static_cast<char*>(buffer) + size), ptr_(buffer_), size_(size) {}

	char* buffer_;
	char* ptr_;
	size_t size_;
};