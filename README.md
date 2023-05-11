`aeldidi/fiber`
==============

`aeldidi/fiber` is a cross platform library implementing
[coroutines](https://en.wikipedia.org/wiki/Coroutine) in C11. It's intended to
be a low-level API suitable for implementing other control flow primitives on
top of such as
[state machines](https://eli.thegreenplace.net/2009/08/29/co-routines-as-an-alternative-to-state-machines/),
[generators](https://en.wikipedia.org/wiki/Generator_(computer_programming)),
[async/await](https://en.wikipedia.org/wiki/Async/await), and anything else
which would benefit from having suspendable and resumable functions.

API
---

Each fiber is represented as an opaque type, and its fields are not exposed.

```c
// An opaque type which represents a fiber.
struct fiber;
```

The size and alignment of this type can be queried, which is how you are
supposed to prepare the memory for it. There is no automatic memory allocation
done by this library, but since C11 is required for its use `aligned_alloc` is
the easiest way to allocate these. Although its impossible to know at compile
time exactly how much memory to allocate (assuming you want to use the library
cross-platform), its reccomended to just allocate your desired stack and
subtract `fiber_size()` from it to use as the memory for the `struct fiber`.

If no stack is desired _and_ minimal memory usage is a desire, it is
reccomended to just look at each architecture's source file and use the size of
the largest `struct fiber`. 

```c
// Returns the size in bytes of struct fiber.
size_t fiber_size();

// Returns the alignment in bytes of struct fiber.
size_t fiber_align();
```

The fiber then needs to be initialized, which `fiber_init` is responsible for:

```c
// Initializes the fiber f given the function and stack provided.
// func will be called with arg as its parameter to begin the fiber.
//
// If func returns, the behaviour of the program is undefined.
void fiber_init(struct fiber* f, void (*func)(void*), void* arg,
		const size_t stack_len, uint8_t* stack);
```

The last two functions then serve as the actual coroutine implementation, with
`fiber_current` returning a pointer to the current `struct fiber` being
executed, and `fiber_switch` switching execution to another fiber, returning
when another fiber switches back to it.

```c
// Saves the current fiber's context in the fiber struct which it was created
// with and returns a pointer to it.
struct fiber* fiber_current();

// Switches to the fiber to.
// to must be initialized with fiber_init before it can be switched to.
// The fiber to must be different from the current fiber (returned by
// fiber_current()).
void
fiber_switch(struct fiber* to);
```

License
-------

0-Clause BSD or Public Domain, at your choice. The 0-Clause BSD license is
included in the `LICENSE` file in the source repo.


