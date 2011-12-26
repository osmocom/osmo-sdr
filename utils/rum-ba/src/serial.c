#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include "serial.h"
#include "utils.h"

int serialOpen(const char* device)
{
	int fd = -1;
	struct termios tios;

	if((fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY)) < 0) {
		fprintf(stderr, "could not open %s: %s\n", device, strerror(errno));
		goto failed;
	}
	bzero(&tios, sizeof(tios));
	tios.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
	tios.c_iflag = IGNPAR;
	tios.c_oflag = 0;
	tios.c_lflag = ICANON;
	tios.c_cc[VMIN] = 1;
	cfmakeraw(&tios);
	if(tcflush(fd, TCIOFLUSH) < 0) {
		fprintf(stderr, "tcflush() failed: %s\n", strerror(errno));
		goto failed;
	}
	if(tcsetattr(fd, TCSAFLUSH, &tios) < 0) {
		fprintf(stderr, "tcsetattr() failed: %s\n", strerror(errno));
		goto failed;
	}
	if(tcflush(fd, TCIOFLUSH) < 0) {
		fprintf(stderr, "tcflush() failed: %s\n", strerror(errno));
		goto failed;
	}

	return fd;

failed:
	if(fd >= 0)
		close(fd);
	return -1;
}

void serialClose(int fd)
{
	if(fd >= 0)
		close(fd);
}

int serialPutC(int fd, char c)
{
	if(write(fd, &c, sizeof(char)) != sizeof(char)) {
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

int serialPutS(int fd, const char* str)
{
	while(*str != '\0') {
		if(serialPutC(fd, *str++) < 0)
			return -1;
	}
	return 0;
}

int serialGetC(int fd, char* c, int timeout)
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

int serialGetS(int fd, char* str, int len, int timeout)
{
	int todo = len;
	uint64_t endTime = getTickCount() + timeout;

	while(todo > 0) {
		uint64_t now = getTickCount();

		if(now < endTime) {
			char c;
			if(serialGetC(fd, &c, endTime - now) < 0)
				return -1;
			//printf("[%02x]", c);
			*str++ = c;
			todo--;
		} else {
			break;
		}
	}

	return (todo > 0) ? -1 : 0;
}

int serialExpect(int fd, const char* str, int timeout)
{
	int todo = strlen(str);
	uint64_t endTime = getTickCount() + timeout;

	while(todo > 0) {
		uint64_t now = getTickCount();

		if(now < endTime) {
			char c;
			if(serialGetC(fd, &c, endTime - now) < 0)
				return -1;
			//printf("[%02x]", c);
			if(c == *str++) {
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
