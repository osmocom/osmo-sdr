#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "sam3u.h"
#include "serial.h"

int sam3uRead32(HANDLE fd, uint32_t address, uint32_t* value)
{
	char str[32];
	sprintf(str, "w%08X,4#", address);
	if(serialPutS(fd, str) < 0)
		return -1;
	if(serialExpect(fd, "\n\r0x", 100) < 0)
		return -1;
	if(serialGetS(fd, str, 8, 100) < 0)
		return -1;
	str[8] = '\0';
	errno = 0;
	*value = strtoll(str, NULL, 16);
	if(errno != 0) {
		fprintf(stderr, "number conversion failed: %s", strerror(errno));
		return -1;
	}
	if(serialExpect(fd, "\n\r>", 100) < 0)
		return -1;
	return 0;
}

int sam3uRead16(HANDLE fd, uint32_t address, uint16_t* value)
{
	char str[32];
	sprintf(str, "h%08X,2#", address);
	if(serialPutS(fd, str) < 0)
		return -1;
	if(serialExpect(fd, "\n\r0x", 100) < 0)
		return -1;
	if(serialGetS(fd, str, 4, 100) < 0)
		return -1;
	str[4] = '\0';
	errno = 0;
	*value = strtol(str, NULL, 16);
	if(errno != 0) {
		fprintf(stderr, "number conversion failed: %s", strerror(errno));
		return -1;
	}
	if(serialExpect(fd, "\n\r>", 100) < 0)
		return -1;
	return 0;
}

int sam3uRead8(HANDLE fd, uint32_t address, uint8_t* value)
{
	char str[32];
	sprintf(str, "o%08X,1#", address);
	if(serialPutS(fd, str) < 0)
		return -1;
	if(serialExpect(fd, "\n\r0x", 100) < 0)
		return -1;
	if(serialGetS(fd, str, 2, 100) < 0)
		return -1;
	str[2] = '\0';
	errno = 0;
	*value = strtol(str, NULL, 16);
	if(errno != 0) {
		fprintf(stderr, "number conversion failed: %s", strerror(errno));
		return -1;
	}
	if(serialExpect(fd, "\n\r>", 100) < 0)
		return -1;
	return 0;
}

int sam3uWrite32(HANDLE fd, uint32_t address, uint32_t value)
{
	char str[32];
	sprintf(str, "W%08X,%08X#", address, value);
	if(serialPutS(fd, str) < 0)
		return -1;
	if(serialExpect(fd, "\n\r>", 100) < 0)
		return -1;
	return 0;
}

int sam3uWrite16(HANDLE fd, uint32_t address, uint16_t value)
{
	char str[32];
	sprintf(str, "H%08X,%04X#", address, value);
	if(serialPutS(fd, str) < 0)
		return -1;
	if(serialExpect(fd, "\n\r>", 100) < 0)
		return -1;
	return 0;
}

int sam3uWrite8(HANDLE fd, uint32_t address, uint8_t value)
{
	char str[32];
	sprintf(str, "O%08X,%02X#", address, value);
	if(serialPutS(fd, str) < 0)
		return -1;
	if(serialExpect(fd, "\n\r>", 100) < 0)
		return -1;
	return 0;
}

int sam3uRun(HANDLE fd, uint32_t address)
{
	char str[32];
	sprintf(str, "G%08X#", address);
	if(serialPutS(fd, str) < 0)
		return -1;
	return 0;
}

int sam3uDetect(HANDLE fd, uint32_t* chipID)
{
	uint8_t c;

	while(serialGetC(fd, &c, 100) >= 0) ;

	if(serialPutS(fd, "T#") < 0)
		return -1;

	if(serialExpect(fd, "\n\r\n\r>", 100) < 0) {
		fprintf(stderr, "did not receive expected answer\n");
		return -1;
	}

	return sam3uRead32(fd, 0x400e0740, chipID);
}

int sam3uReadUniqueID(HANDLE fd, int bank, uint8_t* uniqueID)
{
	uint32_t regBase;
	uint32_t flashBase;
	uint32_t fsr;
	int i;

	switch(bank) {
		case 0:
			regBase = 0x400e0800;
			flashBase = 0x80000;
			break;
		case 1:
			regBase = 0x400e0a00;
			flashBase = 0x100000;
			break;
		default:
			fprintf(stderr, "illegal flash bank");
			return -1;
	}

	if(sam3uWrite32(fd, regBase + 0x04, 0x5a00000e) < 0)
		return -1;

	do {
		if(sam3uRead32(fd, regBase + 0x08, &fsr) < 0)
			return -1;
	} while((fsr & 1) != 0);

	for(i = 0; i < 16; i++) {
		if(sam3uRead8(fd, flashBase + i, uniqueID + i) < 0)
			return -1;
	}

	if(sam3uWrite32(fd, regBase + 0x04, 0x5a00000f) < 0)
		return -1;

	do {
		if(sam3uRead32(fd, regBase + 0x08, &fsr) < 0)
			return -1;
	} while((fsr & 1) == 0);

	return 0;
}

int sam3uFlash(HANDLE fd, int bank, const void* bin, size_t binSize)
{
	uint32_t regBase;
	uint32_t flashBase;
	uint32_t fsr;
	uint32_t flID;
	uint32_t flSize;
	uint32_t flPageSize;
	uint32_t flNbPlane;
	uint8_t buf[8192];
	uint32_t ofs;
	uint32_t todo;
	uint32_t len;
	uint32_t idx;

	switch(bank) {
		case 0:
			regBase = 0x400e0800;
			flashBase = 0x80000;
			break;
		case 1:
			regBase = 0x400e0a00;
			flashBase = 0x100000;
			break;
		default:
			fprintf(stderr, "illegal flash bank");
			return -1;
	}

	printf("reading flash descriptor");

	if(sam3uWrite32(fd, regBase + 0x04, 0x5a000000) < 0)
		goto error;

	do {
		if(sam3uRead32(fd, regBase + 0x08, &fsr) < 0)
			goto error;
	} while((fsr & 1) == 0);


	if(sam3uRead32(fd, regBase + 0x0c, &flID) < 0)
		goto error;
	if(sam3uRead32(fd, regBase + 0x0c, &flSize) < 0)
		goto error;
	if(sam3uRead32(fd, regBase + 0x0c, &flPageSize) < 0)
		goto error;
	if(sam3uRead32(fd, regBase + 0x0c, &flNbPlane) < 0)
		goto error;

	printf(" -- ok\n");
	printf("Flash ID:         0x%08x\n", flID);
	printf("Flash size:       %d Bytes\n", flSize);
	printf("Page size:        %d Bytes\n", flPageSize);
	printf("Number of planes: %d\n", flNbPlane);

#if 1
	printf("erasing flash"); fflush(stdout);

	if(sam3uWrite32(fd, regBase + 0x04, 0x5a000005) < 0)
		goto error;

	do {
		if(sam3uRead32(fd, regBase + 0x08, &fsr) < 0)
			goto error;
	} while((fsr & 1) == 0);

	printf(" -- ok\n");
#endif

	ofs = 0;
	for(todo = binSize; todo > 0; todo -= len) {
		printf("flashing @ 0x%08x", ofs); fflush(stdout);
		len = todo;
		if(len > flPageSize)
			len = flPageSize;
		memset(buf, 0xff, sizeof(buf));
		memcpy(buf, ((uint8_t*)bin) + ofs, len);
		for(idx = 0; idx < flPageSize / 4; idx++) {
			if(sam3uWrite32(fd, flashBase + ofs + (idx * 4), ((uint32_t*)buf)[idx]) < 0)
				goto error;
		}

		if(sam3uWrite32(fd, regBase + 0x04, 0x5a000000 | ((ofs / flPageSize) << 8) | 0x03) < 0)
			goto error;

		do {
			if(sam3uRead32(fd, regBase + 0x08, &fsr) < 0)
				goto error;
		} while((fsr & 1) == 0);

		printf(" -- ok\n");

		ofs += flPageSize;
	}

	ofs = 0;
	for(todo = binSize; todo > 0; todo -= len) {
		printf("verifying @ 0x%08x", ofs); fflush(stdout);
		len = todo;
		if(len > flPageSize)
			len = flPageSize;

		for(idx = 0; idx < flPageSize / 4; idx++) {
			if(sam3uRead32(fd, flashBase + ofs + (idx * 4), ((uint32_t*)buf) + idx) < 0)
				goto error;
		}

		if(memcmp(buf, ((uint8_t*)bin) + ofs, len) == 0)
			printf(" -- ok\n");
		else printf(" -- ERROR\n");

		ofs += flPageSize;
	}

	printf("disabling SAM-BA"); fflush(stdout);
	if(sam3uWrite32(fd, regBase + 0x04, 0x5a000000 | (1 << 8) | 0x0b) < 0)
		goto error;

	do {
		if(sam3uRead32(fd, regBase + 0x08, &fsr) < 0)
			goto error;
	} while((fsr & 1) == 0);

	printf(" -- ok\n");

	return 0;

error:
	printf(" -- ERROR\n");
	return -1;
}
