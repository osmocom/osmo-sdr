/* utility to check a recorded WAV file in fgpa.test_mode=1 for
 * discontinuities in the counter increment/decrements */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define STEP 1

static int check_continuity(uint16_t *samples, uint32_t size)
{
	uint16_t *end = samples + size/sizeof(uint16_t);
	uint16_t *cur;
	int inited = 0;
	uint16_t last_i, last_q;

	for (cur = samples; cur < end; cur += 2) {
		if (!inited) {
			last_i = cur[0];
			last_q = cur[1];
			printf("initial I=%04x, Q=%04x\n",
				last_i, last_q);
			inited = 1;
			continue;
		}

		if (cur[0] != (uint16_t)(last_i - STEP) ||
		    cur[1] != (uint16_t)(last_q + STEP)) {
			fprintf(stderr, "Disocntinuity at %u: "
				"I=%04x/%04x Q=%04x/%04x\n",
				cur - samples, last_i, cur[0],
				last_q, cur[1]);
		}

		last_i = cur[0];
		last_q = cur[1];
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct stat st;
	int fd;
	void *map;
	uint16_t *samples;

	if (argc < 2) {
		fprintf(stderr, "You have to specify a wave file name\n");
		exit(2);
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("opening file");
		exit(2);
	}

	if (fstat(fd, &st) < 0) {
		perror("stat");
		exit(1);
	}

	map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!map) {
		perror("mmap");
		exit(1);
	}

	if (memcmp(map, "RIFF", 4)) {
		fprintf(stderr, "Doeesn't look like a WAV file\n");
		exit(1);
	}

	samples = (uint16_t *)(map + 0x44);
	check_continuity(samples, st.st_size-0x44);

	exit(0);
}
