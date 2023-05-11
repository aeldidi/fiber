#ifndef FIBER_H
#define FIBER_H
#include <inttypes.h>
#include <stddef.h>

// A low-level and complete API for cooperative coroutine scheduling.
// On x86_64, requires GCC-style attributes and the attribute __sysv_abi__
// which should force the function to be called using the System V ABI.

// An opaque type which represents a fiber.
struct fiber;

// Returns the size in bytes of struct fiber.
size_t fiber_size();

// Returns the alignment in bytes of struct fiber.
size_t fiber_align();

// Initializes the fiber f given the function and stack provided.
// func will be called with arg as its parameter to begin the fiber.
//
// If func returns, the behaviour of the program is undefined.
void fiber_init(struct fiber* f, void (*func)(void*), void* arg,
		const size_t stack_len, uint8_t* stack);

// Saves the current fiber's context in the fiber struct which it was created
// with and returns a pointer to it.
struct fiber* fiber_current();

// Switches to the fiber to.
// to must be initialized with fiber_init before it can be switched to.
// The fiber to must be different from the current fiber (returned by
// fiber_current()).
#if defined(__x86_64__)
__attribute__((__sysv_abi__))
#endif
void
fiber_switch(struct fiber* to);

#endif // FIBER_H
