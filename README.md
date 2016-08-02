# Overview
Our main goal was to provide a POC for a developer-friendly asynchronous framework built on top of P0057R3 (http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0057r3.pdf) coroutines. This allowed us not only to experiment with various features of native C++ coroutines, but also explore their potential limitations. The code was tested with MSVC implementation of coroutines.

**The code is not well tested, isn't very optimized and may have a lot of functionality missing.**

# Requirements/priorities

### Having coroutines pinned to threads
For developers, a coroutine looks pretty similar to a regular function. 

    Task<void> myCoroutine() {
      a();

      co_await ...

      b();
    }

If `myCoroutine()` was a regular function, a() and b() would always be run on the same thread. Intuitively developers would expect similar behavior for coroutines. Even though coroutines give us more freedom here, we still want to keep our coroutines more function-like. So we require that for every given coroutine all it's code should be run by the same thread (Executor).

### Little to no understanding of low-level coroutine APIs
We want our framework to be usable by large number of developers who generally have little to no understanding of low-level coroutine APIs. We prefer incorrect code to not compile/explicitly fail at run time rather than result in undefined behavior. 

### Optimize for "linear" (rather than fan-out) asynchronous code
Even though we want good support for coroutines which spawn multiple "concurrent" coroutines (i.e. fan-out), we think that it's much more common for coroutine to spawn another coroutine and block until it's completed (i.e. equivalent to a function call in synchronous code).

    Task<Foo> foo() {
      ...
    } 

    Task<Bar> bar() {
      ...
      auto fooResult = co_await foo();
      ...
    }

So we want such use case to be the most optimized both in terms of usability but also performance.

# Main components
### Promise
`Promise` represents all the state which is stored alongside the coroutine. It also has capacity to store the result/exception returned by the coroutine. 

Users should **never** have to interact with `Promise`s.

### Future
`Future` is a user-facing handle to a running coroutine. It can be used to wait (block current thread/coroutine) until the coroutine is complete and get the result of its execution.
Destroying the `Future`, makes associated coroutine detached, which doesn't cancel its execution.

### Task
`Task` represents an **allocated** coroutine which is not yet started. `Task` can be assigned to a thread (Executor), which starts the coroutine and transforms `Task` into `Future`.

### CallableTask
`CallableTask` represents a **non-allocated** coroutine. It stores references to all arguments for the coroutine function, so generally `CallableTask` object is not supposed to be long-living. `CallableTask` can be transformed into `Task` when the allocator is provided.

### Executor
`Executor` can accept and execute units of work with its `add()` method. In our examples we are only using `ThreadExecutor`, which represents a single thread of execution, running scheduled tasks in FIFO order.

### Allocator
`Allocator` is an interface to a custom memory allocator which can be used to allocate a coroutine and associated `Promise`. In our examples we are only using `StackAllocator`, which requires memory to be allocated/deallocated in LIFO order. This makes it only work for "linear" asynchronous code.

# APIs

To define a new task you can simply write a coroutine which returns `Task<T>`. We also require every coroutine to accept ExecutionContext as its first argument (not having it results in compilation error).

    Task<int> myCoroutine(ExecutionContext, int x) {
      co_return x * 2;
    }

To start a coroutine it needs to be assigned to an `Executor`.

    ThreadExecutor myExecutor;
    auto future = spawn(myExecutor, myCoroutine, 42);
    future.wait();
    assert(future.get() == 42);

`spawn()` API accepts the coroutine function and all arguments which need to be passed to it.

You may have coroutines call and wait for other coroutines.

    Task<int> myOtherCoroutine(ExecutionContext, int x, int y) {
      auto result = co_await call(myCoroutine, x + y);
      assert(result == (x + y) * 2);
      co_return result;
    }

`call()` doesn't start or even create a coroutine. Coroutine is only created and started when someone `co_await`s its return value.

Having such explicit call syntax for calling and waiting allows us having all these coroutines use the same stack allocator. The following code starts `myCoroutine2` and tells it to use pre-allocated stack allocator of fixed size (1024 bytes) for itself and all coroutines it calls into.     

    auto future = spawnWithStack(myExecutor, 1024, myCoroutine2, ...);

We used simple stack allocator for our examples, but it could be extended to a segmented stack or just fallback to malloc if we run out of pre-allocated stack space.

# How can we make it better ? 

### AllocatorPtr
Having all coroutines accept `AllocatorPtr` as their first argument is something we would want to avoid but its currently necessary to pass allocators between coroutines. If we dropped allocator support from the framework, we wouldn't need it. 

### call() function
Same as `AllocatorPtr` argument, we only need `call()` to support custom allocator. 

### Undefined behavior if co_return is missing
We think it could be very confusing for developers that flowing of the end of coroutine without calling co_return is undefined behavior (8.4.4, 4). We would much rather have it be a compilation error (which could be enabled only for some promise types via a trait or something).

### Killing/resuming coroutine from await_suspend
Our code (and also `std::future` implementation in MSVC) heavily relies on the fact that if `await_suspend()` returns true (or is simply void) then it's safe for it to resume/destroy the coroutine (which may happen even before it actually returned true). It would be great for this requirement to be more explicitly highlighted. 

### Missing RVO support
For the "linear" asynchronous code use case we described above, there's no equivalent of RVO optimization which we found possible to implement given current coroutine APIs (actually our current code would result in two move-constructor calls for each coroutine-call).

This may sound like a minor improvement, but we actually know that some code (e.g. https://github.com/facebook/mcrouter) using stack-full coroutines (folly::fibers) was relying on RVO for performance reasons.