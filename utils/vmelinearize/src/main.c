#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "hardware.h"

static int printSyntax()
{
	fprintf(stderr, "Error: Invalid command line!\n\n"
		"syntax: vmelin input-algo input-data > output\n"
		"\n"
		"input-algo: algo-file produced by Diamond\n"
		"input-data: data-file produced by Diamond\n"
		"> output:   linearized file for USB-DFU (this is stdout!)\n");

	return EXIT_FAILURE;
}

static int linearize(const void* algo, size_t algoSize, const void* bin, size_t binSize)
{
    g_ispAlgo = (uint8_t*)algo;
    g_ispAlgoSize = algoSize;
    g_ispData = (uint8_t*)bin;
    g_ispDataSize = binSize;

    return ispEntryPoint();
}

int main(int argc, char* argv[])
{
	int i;
	int res = -1;
	void* bin;
	size_t binSize;
	void* algo;
	size_t algoSize;

	if(argc != 3)
		return printSyntax();

	if((algo = loadFile(argv[1], &algoSize)) == NULL)
		return EXIT_FAILURE;
	if((bin = loadFile(argv[2], &binSize)) == NULL)
		return EXIT_FAILURE;

	res = linearize(algo, algoSize, bin, binSize);

	// add some padding to force the DFU statemachine play through
	for(i = 0; i < 2048; i++)
		printf("%c", 0xff);

	return (res < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
}
