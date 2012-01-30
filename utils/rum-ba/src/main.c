#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial.h"
#include "osmosdr.h"
#include "utils.h"

static int printSyntax()
{
	fprintf(stderr, "Error: Invalid command line!\n\n"
#ifdef WINDOWS
		"syntax: sdr-samba com1 command\n"
#else
		"syntax: sdr-samba /dev/ttyACM0 command\n"
#endif
		"valid commands are:\n"
		"  - detect: detect OsmoSDR and print unique ID\n"
		"  - blink: blink LEDs on board\n"
		"  - ramload image.bin: load image.bin into SRAM and start it\n"
		"  - flashfpga algo.vme data.vme: write data.vme to FPGA FLASH using algo.vme\n"
		"  - flashmcu image.bin: write image.bin to MCU FLASH\n");

	return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
	HANDLE fd;
	int res = -1;
	void* bin;
	size_t binSize;

	if(argc < 3)
		return printSyntax();

	if(strcmp(argv[2], "detect") == 0) {
		if(argc != 3)
			return printSyntax();
		if((fd = serialOpen(argv[1])) == INVALID_HANDLE_VALUE)
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
		if((fd = serialOpen(argv[1])) == INVALID_HANDLE_VALUE)
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
		if((fd = serialOpen(argv[1])) == INVALID_HANDLE_VALUE)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRRamLoad(fd, bin, binSize);
		serialClose(fd);
	} else if(strcmp(argv[2], "flashfpga") == 0) {
		void* algo;
		size_t algoSize;
		if(argc != 5)
			return printSyntax();
		if((algo = loadFile(argv[3], &algoSize)) == NULL)
			return EXIT_FAILURE;
		if((bin = loadFile(argv[4], &binSize)) == NULL)
			return EXIT_FAILURE;
		if((fd = serialOpen(argv[1])) == INVALID_HANDLE_VALUE)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRFlashFPGA(fd, algo, algoSize, bin, binSize);
		serialClose(fd);
	} else if(strcmp(argv[2], "flashmcu") == 0) {
		if(argc != 4)
			return printSyntax();
		if((bin = loadFile(argv[3], &binSize)) == NULL)
			return EXIT_FAILURE;
		if((fd = serialOpen(argv[1])) == INVALID_HANDLE_VALUE)
			return EXIT_FAILURE;
		res = 0;
		if(res >= 0)
			res = osmoSDRDetect(fd);
		if(res >= 0)
			res = osmoSDRFlashMCU(fd, bin, binSize);
		serialClose(fd);
	} else {
		return printSyntax();
	}

	return (res < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
