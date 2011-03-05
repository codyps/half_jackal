
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <unistd.h>

#include <string.h>

#include "error_m.h"

static int serial_conf(int fd, speed_t speed)
{
	return 0;
	struct termios t;
	int ret = tcgetattr(fd, &t);

	if (ret < 0)
		return ret;

	ret = cfsetispeed(&t, speed);
	if (ret < 0)
		return ret;

	ret = cfsetospeed(&t, speed);
	if (ret < 0)
		return ret;

	/* odd parity */
	t.c_cflag |= PARENB | PARODD;

	/* 8 data bits */
	t.c_cflag = (t.c_cflag & ~CSIZE) | CS8;

	/* no flow control */
	t.c_cflag &= ~(CRTSCTS);
	t.c_iflag &= ~(IXON | IXOFF | IXANY);

	/* ignore control lines */
	t.c_cflag |= CLOCAL;

	return tcsetattr(fd, TCSAFLUSH, &t);
}

FILE *term_open(char const *fname)
{
	int sfd = open(fname, O_RDWR);
	if (sfd < 0) {
		ERROR("open: %s: %s", fname, strerror(errno));
		return NULL;
	}

	int ret = serial_conf(sfd, B57600);
	if (ret < 0) {
		ERROR("serial_conf: %s: %s", fname, strerror(errno));
		return NULL;
	}

	FILE *sf = fdopen(sfd, "a+");
	if (!sf) {
		ERROR("fdopen: %s: %s", fname, strerror(errno));
		return NULL;
	}

	return sf;
}

