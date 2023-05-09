#include <stdio.h>
#include <stdlib.h>

#include "../fiber.h"

// returns the next aligned address
#define ALIGN_UP(ptr, alignment)                                              \
	((ptr) + ((alignment) - ((uintptr_t)(ptr) % (alignment))))

struct fiber* main_fiber;

void
fiber_func(void* arg)
{
	(void)arg;
	printf("b\n");
	fiber_switch(main_fiber);
	printf("d\n");
	fiber_switch(main_fiber);
	printf("f\n");
	fiber_switch(main_fiber);
}

uint8_t memory[1 << 16] = {0};

int
main()
{
	struct fiber* f     = ALIGN_UP((void*)memory, fiber_align());
	uint8_t*      stack = (void*)f + fiber_size();
	int           x     = 0;
	fiber_init(f, fiber_func, NULL,
			(1 << 16) - fiber_size() - fiber_align(), stack);
	main_fiber = fiber_current();
	printf("a\n");
	fiber_switch(f);
	printf("c\n");
	fiber_switch(f);
	printf("e\n");
	fiber_switch(f);
	printf("g\n");

	return 0;
}
