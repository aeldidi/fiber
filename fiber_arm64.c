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
	uint64_t sp;

	// In the AAPCS64 ABI used by linux the following registers must be
	// saved:
	// https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst#table-2
	uint64_t r19;
	uint64_t r20;
	uint64_t r21;
	uint64_t r22;
	uint64_t r23;
	uint64_t r24;
	uint64_t r25;
	uint64_t r26;
	uint64_t r27;
	uint64_t r28;

	// Windows also requires we save the following:
	// NOTE: These are 128 bit registers, but only the lower 64 bits are
	//       required to be preserved in Microsoft's ABI.
	// https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions?view=msvc-170#floating-pointsimd-registers
	uint64_t d8;
	uint64_t d9;
	uint64_t d10;
	uint64_t d11;
	uint64_t d12;
	uint64_t d13;
	uint64_t d14;
	uint64_t d15;

	uint64_t fp;
};

struct fiber  main_fiber    = {0};
struct fiber* current_fiber = &main_fiber;

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

	// The AAPCS64 ABI requires the stack to be aligned to 16 bytes.
	// https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst#6451universal-stack-constraints
	f->sp = (uintptr_t)ALIGN_UP(stack + stack_len - 16, 16);
}

// The assembly below makes use of these assumptions:
_Static_assert(offsetof(struct fiber, arg) == 0, "");
_Static_assert(offsetof(struct fiber, pc) == 8, "");
_Static_assert(offsetof(struct fiber, sp) == 16, "");
_Static_assert(offsetof(struct fiber, r19) == 24, "");
_Static_assert(offsetof(struct fiber, r20) == 32, "");
_Static_assert(offsetof(struct fiber, r21) == 40, "");
_Static_assert(offsetof(struct fiber, r22) == 48, "");
_Static_assert(offsetof(struct fiber, r23) == 56, "");
_Static_assert(offsetof(struct fiber, r24) == 64, "");
_Static_assert(offsetof(struct fiber, r25) == 72, "");
_Static_assert(offsetof(struct fiber, r26) == 80, "");
_Static_assert(offsetof(struct fiber, r27) == 88, "");
_Static_assert(offsetof(struct fiber, r28) == 96, "");
_Static_assert(offsetof(struct fiber, d8) == 104, "");
_Static_assert(offsetof(struct fiber, d9) == 112, "");
_Static_assert(offsetof(struct fiber, d10) == 120, "");
_Static_assert(offsetof(struct fiber, d11) == 128, "");
_Static_assert(offsetof(struct fiber, d12) == 136, "");
_Static_assert(offsetof(struct fiber, d13) == 144, "");
_Static_assert(offsetof(struct fiber, d14) == 152, "");
_Static_assert(offsetof(struct fiber, d15) == 160, "");
_Static_assert(offsetof(struct fiber, fp) == 168, "");

struct fiber*
fiber_current()
{
	return current_fiber;
}

__attribute__((__noinline__, __naked__)) void
fiber_switch(struct fiber* to)
{
	__asm__("str lr, [%[current_fiber], #8]\n\t" // pc = lr
		"mov x1, sp\n\t"
		"str x1, [%[current_fiber], #16]\n\t" // save sp
		"str fp, [%[current_fiber], #168]\n\t"

		// Save all registers the ABI says to
		"str x19, [%[current_fiber], #24]\n\t"
		"str x20, [%[current_fiber], #32]\n\t"
		"str x21, [%[current_fiber], #40]\n\t"
		"str x22, [%[current_fiber], #48]\n\t"
		"str x23, [%[current_fiber], #56]\n\t"
		"str x24, [%[current_fiber], #64]\n\t"
		"str x25, [%[current_fiber], #72]\n\t"
		"str x26, [%[current_fiber], #80]\n\t"
		"str x27, [%[current_fiber], #88]\n\t"
		"str x28, [%[current_fiber], #96]\n\t"

		"str d8, [%[current_fiber], #104]\n\t"
		"str d9, [%[current_fiber], #112]\n\t"
		"str d10, [%[current_fiber], #120]\n\t"
		"str d11, [%[current_fiber], #128]\n\t"
		"str d12, [%[current_fiber], #136]\n\t"
		"str d13, [%[current_fiber], #144]\n\t"
		"str d14, [%[current_fiber], #152]\n\t"
		"str d15, [%[current_fiber], #160]\n\t"
			: // no outputs
			// inputs:
			: [current_fiber] "r"(current_fiber)
			// clobbers
			: "memory", "x1");

	// set current_fiber->pc to this function's return address and save the
	// current state of the stack, then set current_fiber to the one passed
	// to the function.
	//
	// This needs to be separate from the other __asm__ block so when the
	// various inputs are loaded, they happen after current_fiber has been
	// switched.
	__asm__("str x0, %[current_fiber]\n\t" // current_fiber = to
			// outputs:
			: [current_fiber] "=m"(current_fiber)
			: // no inputs
			: // no clobbers
	);

	// I can't simply list the memory locations as inputs, since there
	// aren't enough non-volatile registers for gcc to be able to place
	// everything in, so I just use offsets from the base address, using
	// static asserts to confirm they are correct.
	__asm__ __volatile__(
			"mov x1, x0\n\t" // x1 = to

			// We can unconditionally set x0 to arg since on
			// function entry, this will make it the first
			// argument, and on subsequent switches the code
			// assumes x0 was clobbered anyway, since the ABI says
			// x0 is caller-saved.
			"ldr x0, [%[current_fiber]]\n" // arg = x0

			// restore all registers the ABI says to
			"ldr x19, [%[current_fiber], #24]\n\t"
			"ldr x20, [%[current_fiber], #32]\n\t"
			"ldr x21, [%[current_fiber], #40]\n\t"
			"ldr x22, [%[current_fiber], #48]\n\t"
			"ldr x23, [%[current_fiber], #56]\n\t"
			"ldr x24, [%[current_fiber], #64]\n\t"
			"ldr x25, [%[current_fiber], #72]\n\t"
			"ldr x26, [%[current_fiber], #80]\n\t"
			"ldr x27, [%[current_fiber], #88]\n\t"
			"ldr x28, [%[current_fiber], #96]\n\t"

			"ldr d8, [%[current_fiber], #104]\n\t"
			"ldr d9, [%[current_fiber], #112]\n\t"
			"ldr d10, [%[current_fiber], #120]\n\t"
			"ldr d11, [%[current_fiber], #128]\n\t"
			"ldr d12, [%[current_fiber], #136]\n\t"
			"ldr d13, [%[current_fiber], #144]\n\t"
			"ldr d14, [%[current_fiber], #152]\n\t"
			"ldr d15, [%[current_fiber], #160]\n\t"

			// set the new stack
			"ldr x3, [%[current_fiber], #16]\n\t"
			"mov sp, x3\n\t"
			"ldr x3, [%[current_fiber], #168]\n\t"
			"mov fp, x3\n\t"

			// jump to the new pc
			"ldr x3, [%[current_fiber], #8]\n\t"
			"br x3"

			: // no outputs

			// inputs:
			: [current_fiber] "r"(current_fiber)

			// clobbers:
			// I initially thought that we wouldn't have to
			// list these, since we're following the calling
			// convention by saving all required registers,
			// but listing them here also means the compiler
			// won't use these registers to place
			// current_fiber in.
			: "x19", "x20", "x21", "x22", "x23", "x24", "x24",
			"x25", "x26", "x27", "x28", "d8", "d9", "d10", "d11",
			"d12", "d13", "d14", "d15", "x3", "memory");
}
