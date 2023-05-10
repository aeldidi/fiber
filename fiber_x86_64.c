#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "fiber.h"

// Requires GCC-style inline assembly.

// TODO: Address sanitizer doesn't like when you use the stack with this. See
//       https://github.com/kurocha/concurrent/blob/6eee988ba7263f017a8d74560afde2f0396c1370/source/Concurrent/Fiber.cpp#L46-L70
//       for an example of how to integrate address sanitizer into this

// returns the next aligned address
#define ALIGN_UP(ptr, alignment)                                              \
	((ptr) + ((alignment) - ((uintptr_t)(ptr) % (alignment))))

struct fiber {
	void*    arg;
	uint64_t pc;
	uint64_t rsp;
	uint64_t rbp;

	// The System V ABI says these registers should be preserved:
	// https://www.uclibc.org/docs/psABI-x86_64.pdf (Figure 3.4)
	// We don't need to save the x87 control word, because C assumes this
	// doesn't change anyway.
	uint64_t rbx;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	// we don't need to preserve the required xmm registers since we're
	// giving all these functions the __attribute__((sysv_abi)), which
	// means even on Windows these functions will use the System V ABI.
};

struct fiber  _main_fiber    = {0};
struct fiber* _current_fiber = &_main_fiber;

size_t
fiber_size()
{
	return sizeof(struct fiber);
}

size_t
fiber_align()
{
	return _Alignof(struct fiber);
}

void
fiber_init(struct fiber* f, void (*func)(void*), void* arg,
		const size_t stack_len, uint8_t* stack)
{
	f->arg = arg;
	f->pc  = (uintptr_t)func;

	// The System V ABI requires the stack to be aligned to 16 bytes.
	f->rsp = (uintptr_t)ALIGN_UP(stack + stack_len - 16, 16);
}

struct fiber*
fiber_current()
{
	return _current_fiber;
}

// The assembly below makes use of these assumptions:
_Static_assert(offsetof(struct fiber, arg) == 0, "");
_Static_assert(offsetof(struct fiber, pc) == 8, "");
_Static_assert(offsetof(struct fiber, rsp) == 16, "");
_Static_assert(offsetof(struct fiber, rbp) == 24, "");
_Static_assert(offsetof(struct fiber, rbx) == 32, "");
_Static_assert(offsetof(struct fiber, r12) == 40, "");
_Static_assert(offsetof(struct fiber, r13) == 48, "");
_Static_assert(offsetof(struct fiber, r14) == 56, "");
_Static_assert(offsetof(struct fiber, r15) == 64, "");

// by giving this function the sysv_abi attribute, the first parameter will be
// passed in rdi, even on Windows.
__attribute__((noinline, naked, sysv_abi)) void
fiber_switch(struct fiber* to)
{
	__asm__("popq %%rcx\n\t" // load return address into rcx
		"andq $0xfffffffffffffff0, %%rsp\n\t"  // re-align the stack
		"movq %%rcx, 8(%[current_fiber])\n\t"  // pc = lr
		"movq %%rsp, 16(%[current_fiber])\n\t" // save sp
		"movq %%rbp, 24(%[current_fiber])\n\t"

		// Save all registers the ABI says to
		"movq %%rbx, 32(%[current_fiber])\n\t"
		"movq %%r12, 40(%[current_fiber])\n\t"
		"movq %%r13, 48(%[current_fiber])\n\t"
		"movq %%r14, 56(%[current_fiber])\n\t"
		"movq %%r15, 64(%[current_fiber])\n\t"

			: // no outputs
			// inputs:
			: [current_fiber] "r"(_current_fiber)
			// clobbers
			: "rcx", "memory");

	// set current_fiber->pc to this function's return address and save the
	// current state of the stack, then set current_fiber to the one passed
	// to the function.
	//
	// This needs to be separate from the other __asm__ block so when the
	// various inputs are loaded, they happen after current_fiber has been
	// switched.
	__asm__("movq %%rdi, %[current_fiber]\n\t" // current_fiber = to
			// outputs:
			: [current_fiber] "=m"(_current_fiber)
			: // no inputs
			: // no clobbers
	);

	// I can't simply list the memory locations as inputs, since there
	// aren't enough non-volatile registers for gcc to be able to place
	// everything in, so I just use offsets from the base address, using
	// static asserts to confirm they are correct.
	__asm__ __volatile__(
			"movq %%rdi, %%rcx\n\t" // rcx = to

			// We can unconditionally set rdi to arg since on
			// function entry, this will make it the first
			// argument, and on subsequent switches the code
			// assumes rdi was clobbered anyway, since the ABI says
			// rdi is caller-saved.
			"movq 0(%[current_fiber]), %%rdi\n" // rdi = arg

			// restore all registers the ABI says to
			"movq 32(%[current_fiber]), %%rbx\n\t"
			"movq 40(%[current_fiber]), %%r12\n\t"
			"movq 48(%[current_fiber]), %%r13\n\t"
			"movq 56(%[current_fiber]), %%r14\n\t"
			"movq 64(%[current_fiber]), %%r15\n\t"

			// set the new stack
			"movq 16(%[current_fiber]), %%rsp\n\t"
			"movq 24(%[current_fiber]), %%rbp\n\t"

			// jump to the new pc
			"jmp *8(%[current_fiber])\n\t"

			: // no outputs

			// inputs:
			: [current_fiber] "r"(_current_fiber)

			// clobbers:
			// I initially thought that we wouldn't have to
			// list these, since we're following the calling
			// convention by saving all required registers,
			// but listing them here also means the compiler
			// won't use these registers to place
			// current_fiber in.
			: "rcx", "rdi", "rbx", "r12", "r13", "r14", "r15",
			"memory");
}
