#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fiber.h"

// Requires GCC-style inline assembly.

// returns the next aligned address
#define ALIGN_UP(ptr, alignment)                                              \
	((ptr) + ((alignment) - ((uintptr_t)(ptr) % (alignment))))

struct fiber {
	void*    arg;
	uint64_t pc;
	uint64_t sp;

	// In the AAPCS64 ABI used by linux the following registers must be
	// saved:
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
	uint64_t d8;
	uint64_t d9;
	uint64_t d10;
	uint64_t d11;
	uint64_t d12;
	uint64_t d13;
	uint64_t d14;
	uint64_t d15;
};

struct fiber* current_fiber = NULL;
struct fiber  main_fiber    = {0};

size_t
fiber_size()
{
	return sizeof(struct fiber);
}

// TODO: the stack must have 16 byte alignment. What do we do if stack_len is
//       less than that?
// TODO: align the stack to 16 bytes
void
fiber_init(struct fiber* f, void (*func)(void*), void* arg,
		const size_t stack_len, uint8_t* stack)
{
	current_fiber = fiber_current();

	f->arg = arg;
	f->pc  = (uintptr_t)func;
	f->sp  = (uintptr_t)ALIGN_UP(stack + stack_len - 16, 16);
}

__attribute__((noinline, naked)) struct fiber*
fiber_current()
{
	__asm__ __volatile__(
			// if current_fiber != NULL goto 1
			"cbnz %[current_fiber], 1f\n\t"
			"str %[current_fiber], %[main_fiber]\n"
			"1:\n\t"
			"str %[ra], %[old_pc]\n\t" // save return address into pc
			"mov x0, sp\n\t"
			"str x0, %[old_sp]\n\t" // save sp

			// Save all registers the ABI says to
			"str x19, %[r19]\n\t"
			"str x20, %[r20]\n\t"
			"str x21, %[r21]\n\t"
			"str x22, %[r22]\n\t"
			"str x23, %[r23]\n\t"
			"str x24, %[r24]\n\t"
			"str x25, %[r25]\n\t"
			"str x26, %[r26]\n\t"
			"str x27, %[r27]\n\t"
			"str x28, %[r28]\n\t"

			"str d8, %[d8]\n\t"
			"str d9, %[d9]\n\t"
			"str d10, %[d10]\n\t"
			"str d11, %[d11]\n\t"
			"str d12, %[d12]\n\t"
			"str d13, %[d13]\n\t"
			"str d14, %[d14]\n\t"
			"str d15, %[d15]\n\t"

			// now return current_fiber
			"mov x0, %[current_fiber]\n\t"
			"ret"

			// outputs:
			: [old_pc] "=m"(current_fiber->pc),
			[old_sp] "=m"(current_fiber->sp),
			[main_fiber] "=m"(main_fiber),
			[r19] "=m"(current_fiber->r19),
			[r20] "=m"(current_fiber->r20),
			[r21] "=m"(current_fiber->r21),
			[r22] "=m"(current_fiber->r22),
			[r23] "=m"(current_fiber->r23),
			[r24] "=m"(current_fiber->r24),
			[r25] "=m"(current_fiber->r25),
			[r26] "=m"(current_fiber->r26),
			[r27] "=m"(current_fiber->r27),
			[r28] "=m"(current_fiber->r28),
			[d8] "=m"(current_fiber->d8),
			[d9] "=m"(current_fiber->d9),
			[d10] "=m"(current_fiber->d10),
			[d11] "=m"(current_fiber->d11),
			[d12] "=m"(current_fiber->d12),
			[d13] "=m"(current_fiber->d13),
			[d14] "=m"(current_fiber->d14),
			[d15] "=m"(current_fiber->d15)

			// inputs:
			: [fp] "r"(__builtin_frame_address(0)),
			[ra] "r"(__builtin_return_address(0)),
			[current_fiber] "r"(current_fiber)

			: // no clobbers
	);
}

__attribute__((noinline, naked)) void
fiber_switch(struct fiber* to)
{
	// This needs to be separate so when the various inputs are loaded,
	// they happen after this load.
	__asm__("str x0, %[current_fiber]\n\t" // current_fiber = f
					       // outputs:
			: [current_fiber] "=m"(current_fiber)
			: // no inputs
			: // no clobbers
	);

	__asm__ __volatile__(              //
			"mov x1, x0\n\t"   // x1 = f
			"mov x0, %[arg]\n" // arg = x0

			// restore all registers the ABI says to
			"ldr x19, %[r19]\n\t"
			"ldr x20, %[r20]\n\t"
			"ldr x21, %[r21]\n\t"
			"ldr x22, %[r22]\n\t"
			"ldr x23, %[r23]\n\t"
			"ldr x24, %[r24]\n\t"
			"ldr x25, %[r25]\n\t"
			"ldr x26, %[r26]\n\t"
			"ldr x27, %[r27]\n\t"
			"ldr x28, %[r28]\n\t"

			"ldr d8, %[d8]\n\t"
			"ldr d9, %[d9]\n\t"
			"ldr d10, %[d10]\n\t"
			"ldr d11, %[d11]\n\t"
			"ldr d12, %[d12]\n\t"
			"ldr d13, %[d13]\n\t"
			"ldr d14, %[d14]\n\t"
			"ldr d15, %[d15]\n\t"

			// set the new stack
			"ldr x3, %[new_sp]\n\t"
			"mov sp, x3\n\t"

			// jump to the new pc
			"ldr x3, %[new_pc]\n\t"
			"br x3"

			: // no outputs

			// inputs:
			: [arg] "r"(current_fiber->arg),
			[r19] "m"(current_fiber->r19),
			[r20] "m"(current_fiber->r20),
			[r21] "m"(current_fiber->r21),
			[r22] "m"(current_fiber->r22),
			[r23] "m"(current_fiber->r23),
			[r24] "m"(current_fiber->r24),
			[r25] "m"(current_fiber->r25),
			[r26] "m"(current_fiber->r26),
			[r27] "m"(current_fiber->r27),
			[r28] "m"(current_fiber->r28),
			[d8] "m"(current_fiber->d8),
			[d9] "m"(current_fiber->d9),
			[d10] "m"(current_fiber->d10),
			[d11] "m"(current_fiber->d11),
			[d12] "m"(current_fiber->d12),
			[d13] "m"(current_fiber->d13),
			[d14] "m"(current_fiber->d14),
			[d15] "m"(current_fiber->d15),
			[new_pc] "m"(current_fiber->pc),
			[new_sp] "m"(current_fiber->sp)

			: // no clobbers
	);
}
