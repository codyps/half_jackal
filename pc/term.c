#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <unistd.h>

#include <string.h>

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

	cfmakeraw(&t);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;

	/* odd parity */
	t.c_cflag |= PARENB | PARODD;

	/* 8 data bits */
	t.c_cflag = (t.c_cflag & ~CSIZE) | CS8;

	/* no flow control */
	t.c_cflag &= ~(CRTSCTS);
	t.c_iflag &= ~(IXON | IXOFF | IXANY);

	/* ignore control lines */
	t.c_cflag |= CLOCAL;

	/* hupcl */

	return tcsetattr(fd, TCSAFLUSH, &t);
}

FILE *term_open(char const *fname)
{
	int sfd = open(fname, O_RDWR);
	if (sfd < 0) {
		return NULL;
	}

	int ret = serial_conf(sfd, B57600);
	if (ret < 0) {
		return NULL;
	}

	FILE *sf = fdopen(sfd, "a+");
	if (!sf) {
		return NULL;
	}

	return sf;
}

