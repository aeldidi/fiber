#include <stdio.h>
#include <stdlib.h>

#include "../fiber.h"

int           x = 0;
struct fiber* main_fiber;

void
fiber_func(void* arg)
{
	(void)arg;
	x += 1;
	printf("here!\n");
	fiber_switch(main_fiber);
}

int
main()
{
	void* memory = calloc(1 << 16, 1);
	if (memory == NULL) {
		abort();
	}

	struct fiber* f     = memory;
	uint8_t*      stack = memory + fiber_size();
	fiber_init(f, fiber_func, NULL, (1 << 16) - fiber_size(), stack);
	main_fiber = fiber_current();
	if (x == 0) {
		fiber_switch(f);
	}
	free(memory);
}
