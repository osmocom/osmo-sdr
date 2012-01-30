#ifdef WINDOWS

#include <stdio.h>
#include "serial.h"

static void printErr(const char* text, int err)
{
	char errorBuf[256];
	char* buf = errorBuf;
	size_t max = sizeof(errorBuf) - 1;
	memset(errorBuf, 0x00, sizeof(errorBuf));

	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), LANG_NEUTRAL, buf, (DWORD)max, NULL);
	fprintf(stderr, "%s: %s\n", text, errorBuf);
	fflush(stderr);
}

HANDLE serialOpen(const char* dev)
{
	char str[64];
	HANDLE fd;
	int err;

	snprintf(str, sizeof(str), "\\\\.\\%s", dev);
	fd = CreateFileA(str, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(fd == INVALID_HANDLE_VALUE) {
		err = GetLastError();
		if(err == ERROR_FILE_NOT_FOUND) {
			fprintf(stderr, "could not open %s: serial port does not exist\n", dev);
		} else {
			snprintf(str, sizeof(str), "could not open %s", dev);
			printErr(str, err);
		}
		goto failed;
	}

	if(serialSetBaudrate(fd, 115200) < 0)
		goto failed;

	return fd;

failed:
	if(fd != INVALID_HANDLE_VALUE)
		CloseHandle(fd);
	return INVALID_HANDLE_VALUE;
}

void serialClose(HANDLE fd)
{
	if(fd != INVALID_HANDLE_VALUE)
		CloseHandle(fd);
}

int serialSetBaudrate(HANDLE fd, int baudrate)
{
	DCB dcb;

	memset(&dcb, 0x00, sizeof(dcb));
	dcb.DCBlength = sizeof(dcb);
	if(!GetCommState(fd, &dcb)) {
		printErr("error getting serial port state", GetLastError());
		return -1;
	}
	dcb.BaudRate = baudrate;
	dcb.ByteSize = 8;
	dcb.StopBits = ONESTOPBIT;
	dcb.Parity = NOPARITY;
	dcb.fBinary = 1;
	dcb.fOutxCtsFlow = 0;
	dcb.fOutxDsrFlow = 0;
	dcb.fOutX = 0;
	dcb.fInX = 0;
	dcb.fNull = 0;
	dcb.fTXContinueOnXoff = 0;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fDsrSensitivity = 0;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	if(!SetCommState(fd, &dcb)) {
		printErr("error setting serial port state", GetLastError());
		return -1;
	}

	return 0;
}

int serialPutC(HANDLE fd, uint8_t c)
{
	DWORD bytesWritten;

	if(!WriteFile(fd, &c, 1, &bytesWritten, NULL)) {
		printErr("error writing to serial port", GetLastError());
		return -1;
	}

	return 1;
}

int serialGetC(HANDLE fd, uint8_t* c, int timeout)
{
	COMMTIMEOUTS timeouts;
	unsigned long res;
	COMSTAT comStat;
	DWORD errors;

	if(!ClearCommError(fd, &errors, &comStat)) {
		printErr("could not reset comm errors", GetLastError());
		return -1;
	}

	if(!GetCommTimeouts(fd, &timeouts)) {
		printErr("error getting comm timeouts", GetLastError());
		return -1;
	}
	timeouts.ReadIntervalTimeout = timeout;
	timeouts.ReadTotalTimeoutConstant = timeout;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	if(!SetCommTimeouts(fd, &timeouts)) {
		printErr("error setting comm timeouts", GetLastError());
		return -1;
	}

	if(!ReadFile(fd, c, 1, &res, NULL)) {
		printErr("error reading from serial port", GetLastError());
		return -1;
	}

	if(res != 1)
		return -1;

	return *c;
}

#else // #ifdef WINDOWS

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include "serial.h"

HANDLE serialOpen(const char* dev)
{
	HANDLE fd;

	if((fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
		fprintf(stderr, "could not open %s: %s\n", dev, strerror(errno));
		goto failed;
	}
	if(tcflush(fd, TCIOFLUSH) < 0) {
		fprintf(stderr, "tcflush() failed: %s\n", strerror(errno));
		goto failed;
	}
	if(serialSetBaudrate(fd, 115200) < 0)
		goto failed;

	return fd;

failed:
	if(fd >= 0)
		close(fd);
	return INVALID_HANDLE_VALUE;
}

void serialClose(HANDLE fd)
{
	if(fd != INVALID_HANDLE_VALUE)
		close(fd);
}

int serialSetBaudrate(HANDLE fd, int baudrate)
{
	struct termios tios;

	bzero(&tios, sizeof(tios));
	switch(baudrate) {
		case 230400:
			tios.c_cflag = B230400;
			break;

		case 460800:
			tios.c_cflag = B460800;
			break;

		case 921600:
			tios.c_cflag = B921600;
			break;

		default:
			tios.c_cflag = B115200;
			break;
	}
	tios.c_cflag |= CS8 | CLOCAL | CREAD;
	tios.c_iflag = IGNPAR;
	tios.c_oflag = 0;
	tios.c_lflag = ICANON;
	tios.c_cc[VMIN] = 1;
	cfmakeraw(&tios);

	if(tcsetattr(fd, TCSAFLUSH, &tios) < 0) {
		fprintf(stderr, "tcsetattr() failed: %s\n", strerror(errno));
		return -1;
	}
	if(tcflush(fd, TCIOFLUSH) < 0) {
		fprintf(stderr, "tcflush() failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int serialPutC(HANDLE fd, uint8_t c)
{
	if(write(fd, &c, sizeof(uint8_t)) != sizeof(uint8_t)) {
		if(errno == EAGAIN) {
			struct pollfd pollfd;
			int res;
			pollfd.fd = fd;
			pollfd.events = POLLOUT;
			pollfd.revents = 0;
			do {
				res = poll(&pollfd, 1, 250);
			} while((res < 0) && (errno == EINTR));
			if(res > 0) {
				if(write(fd, &c, sizeof(char)) != sizeof(char)) {
					fprintf(stderr, "write failed: %s\n", strerror(errno));
					return -1;
				} else {
					return 0;
				}
			} else if(res == 0) {
				fprintf(stderr, "write failed: %s\n", strerror(errno));
				return -1;
			} else {
				fprintf(stderr, "poll failed: %s\n", strerror(errno));
				return -1;
			}
		} else {
			fprintf(stderr, "write failed: %s\n", strerror(errno));
			return -1;
		}
	} else {
		return 0;
	}
}

int serialGetC(HANDLE fd, uint8_t* c, int timeout)
{
	struct pollfd pollfd;
	int res;

	pollfd.fd = fd;
	pollfd.events = POLLIN;
	pollfd.revents = 0;

	do {
		res = poll(&pollfd, 1, timeout);
	} while((res < 0) && (errno == EINTR));

	if(res > 0) {
		if(read(fd, c, sizeof(char)) != sizeof(char)) {
			fprintf(stderr, "read failed: %s\n", strerror(errno));
			return -1;
		} else {
			return 0;
		}
	} else if(res == 0) {
		// timeout
		return -1;
	}

	fprintf(stderr, "poll failed: %s\n", strerror(errno));
	return -1;
}

#endif // #ifdef WINDOWS

int serialPutS(HANDLE fd, const char* str)
{
	while(*str != '\0') {
		if(serialPutC(fd, *((uint8_t*)str)) < 0)
			return -1;
		str++;
	}
	return 0;
}

int serialGetS(HANDLE fd, char* str, int len, int timeout)
{
	int todo = len;
	uint64_t endTime = getTickCount() + timeout;

	while(todo > 0) {
		uint64_t now = getTickCount();

		if(now < endTime) {
			uint8_t c;
			if(serialGetC(fd, &c, endTime - now) < 0)
				return -1;
			*((uint8_t*)str) = c;
			str++;
			todo--;
		} else {
			break;
		}
	}

	return (todo > 0) ? -1 : 0;
}

int serialExpect(HANDLE fd, const char* str, int timeout)
{
	int todo = strlen(str);
	uint64_t endTime = getTickCount() + timeout;

	while(todo > 0) {
		uint64_t now = getTickCount();

		if(now < endTime) {
			uint8_t c;
			if(serialGetC(fd, &c, endTime - now) < 0)
				return -1;
			if(c == (uint8_t)(*str++)) {
				todo--;
				continue;
			} else {
				return -1;
			}
		} else {
			break;
		}
	}

	return (todo > 0) ? -1 : 0;
}
