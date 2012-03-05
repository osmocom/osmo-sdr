#include <time.h>
#include "utils.h"

uint64_t getTickCount()
{
	struct timespec t;

	if(clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (((uint64_t)t.tv_sec) * 1000) + (((uint64_t)t.tv_nsec) / 1000000);
}
