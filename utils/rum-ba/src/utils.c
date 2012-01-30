#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "utils.h"

#ifdef WINDOWS

#include <windows.h>

uint64_t getTickCount()
{
	return GetTickCount();
}

#else // WINDOWS

uint64_t getTickCount()
{
	struct timespec t;

	if(clock_gettime(CLOCK_MONOTONIC, &t) != 0)
		return 0;
	return (((uint64_t)t.tv_sec) * 1000) + (((uint64_t)t.tv_nsec) / 1000000);
}

#endif

void* loadFile(const char* filename, size_t* size)
{
	void* result = NULL;
	struct stat statbuf;
	FILE* f = NULL;

	if(stat(filename, &statbuf) < 0) {
		fprintf(stderr, "could not stat() file %s: %s\n", filename, strerror(errno));
		goto failed;
	}

	if((result = calloc(1, statbuf.st_size)) == NULL) {
#ifdef WINDOWS
		fprintf(stderr, "failed to allocate %u bytes of memory\n", (size_t)statbuf.st_size);
#else
		fprintf(stderr, "failed to allocate %zu bytes of memory\n", (size_t)statbuf.st_size);
#endif
		goto failed;
	}
	if((f = fopen(filename, "rb")) == NULL) {
		fprintf(stderr, "failed to open %s: %s\n", filename, strerror(errno));
		goto failed;
	}
	if(fread(result, 1, statbuf.st_size, f) != statbuf.st_size) {
		fprintf(stderr, "could not read all bytes: %s\n", strerror(errno));
		goto failed;
	}

	fclose(f);
	*size = (size_t)statbuf.st_size;
	return result;

failed:
	if(f != NULL)
		fclose(f);
	return NULL;
}
