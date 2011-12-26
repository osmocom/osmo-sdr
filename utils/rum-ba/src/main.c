#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial.h"
#include "osmosdr.h"
#include "utils.h"

static int printSyntax()
{
	fprintf(stderr, "Error: Invalid command line!\n\n"
		"syntax: sdr-samba /dev/ttyACM0 command\n"
		"valid commands are:\n"
		"  - detect: detect OsmoSDR and print unique ID\n"
		"  - blink: blink LEDs on board\n"
		"  - ramload image.bin: load image.bin into SRAM and start it\n"
		"  - flash image.bin: write image.bin to FLASH\n");

	return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
	int fd;
	int res = -1;
	void* bin;
	size_t binSize;

	if(argc < 3)
		return printSyntax();

	if(strcmp(argv[2], "detect") == 0) {
		if(argc != 3)
			return printSyntax();
		if((fd = serialOpen(argv[1])) < 0)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRPrintUID(fd, 0);
		if(res >= 0)
			res = osmoSDRPrintUID(fd, 1);
		serialClose(fd);
	} else if(strcmp(argv[2], "blink") == 0) {
		if(argc != 3)
			return printSyntax();
		if((fd = serialOpen(argv[1])) < 0)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRBlink(fd);
		serialClose(fd);
	} else if(strcmp(argv[2], "ramload") == 0) {
		if(argc != 4)
			return printSyntax();
		if((bin = loadFile(argv[3], &binSize)) == NULL)
			return EXIT_FAILURE;
		if((fd = serialOpen(argv[1])) < 0)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRRamLoad(fd, bin, binSize);
		serialClose(fd);
	} else if(strcmp(argv[2], "flash") == 0) {
		if(argc != 4)
			return printSyntax();
		if((bin = loadFile(argv[3], &binSize)) == NULL)
			return EXIT_FAILURE;
		if((fd = serialOpen(argv[1])) < 0)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRFlash(fd, bin, binSize);
		serialClose(fd);
	} else {
		return printSyntax();
	}

	return (res < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
