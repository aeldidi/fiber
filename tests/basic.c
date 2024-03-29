#include <stdio.h>
#include <stdlib.h>

#include "../fiber.h"

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

int
main()
{
	void* memory = aligned_alloc(fiber_align(), fiber_size() + (1 << 16));
	struct fiber* f     = memory;
	uint8_t*      stack = memory + fiber_size();
	int           x     = 0;
	fiber_init(f, fiber_func, NULL, (1 << 16) - fiber_size(), stack);
	main_fiber = fiber_current();
	printf("a\n");
	fiber_switch(f);
	printf("c\n");
	fiber_switch(f);
	printf("e\n");
	fiber_switch(f);
	printf("g\n");

	free(memory);
	return 0;
}
