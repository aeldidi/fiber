#ifndef FIBER_H
#define FIBER_H
#include <inttypes.h>
#include <stddef.h>

// A low-level and complete API for cooperative coroutine scheduling.

// An opaque type which represents a fiber.
struct fiber;

// Returns the size in bytes of struct fiber for the backend being used.
size_t fiber_size();

// Initializes the fiber f given the function and stack provided.
// func will be called with arg as its parameter to begin the fiber.
//
// It is undefined behaviour if any of the following happen:
// - func returns
// - stack_len is less than 2000
void fiber_init(struct fiber* f, void (*func)(void*), void* arg,
		const size_t stack_len, uint8_t* stack);

// Saves the current fiber's context in the fiber struct which it was created
// with and returns a pointer to it.
struct fiber* fiber_current();

// Switches to the fiber to.
// to must be initialized with fiber_init before it can be switched to.
void fiber_switch(struct fiber* to);

#endif // FIBER_H
