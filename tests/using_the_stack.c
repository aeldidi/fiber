#include <stdio.h>
#include <stdlib.h>

#include "../fiber.h"

struct fiber* main_fiber;
struct fiber* other_fiber;

void
fiber_func(void* arg)
{
	(void)arg;
	int y       = 7;
	other_fiber = fiber_current();
	fiber_switch(main_fiber);
	printf("%d\n", y);
	exit(EXIT_SUCCESS);
}

uint8_t memory[1 << 16] = {0};

int
main()
{
	struct fiber* f     = (void*)memory;
	uint8_t*      stack = memory + fiber_size();
	fiber_init(f, fiber_func, NULL, (1 << 16) - fiber_size(), stack);
	main_fiber = fiber_current();
	fiber_switch(f);
	fiber_switch(f);
	return 0;
}
