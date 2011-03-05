#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <termios.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "frame_async.h"
#include "../hj_proto.h"


struct bytebuf {
	char *buf;
	size_t data_len;
	size_t mem_len;
};

struct bytebuf *bytebuf_mk(size_t initial_sz)
{
	struct bytebuf *s = malloc(sizeof(*s));
	if (!s)
		return NULL;

	s->buf = malloc(initial_sz);
	if (!s->buf)
		return NULL;

	s->mem_len = initial_sz;
	s->data_len = 0;
	return s;
}

struct bytebuf *bytebuf_append(struct bytebuf *s, char c) {
	return 0;
}


void send_req_info(FILE *out)
{
	struct hjb_pkt_req_info ri = HJB_PKT_REQ_INFO_INITIALIZER;
	frame_send(out, &ri, HJB_PL_REQ_INFO);
}

void print_hj_motor_info(struct hj_pktc_motor_info *inf, FILE *out)
{
	fprintf(out, "current: %"PRIu16" enc_ct: %"PRIu32
			" pwr: %"PRIi16" vel: %"PRIi16,
			ntohs(inf->current),
			ntohl(inf->enc_ct),
			(int16_t)ntohs(inf->pwr),
			(int16_t)ntohs(inf->vel));
}

#define ERROR(fmt, ...) _ERROR(__FILE__,__LINE__,__func__,fmt,##__VA_ARGS__)

#define _ERROR(file, line, func, fmt, ...) \
	__ERROR(file, line, func, fmt, ##__VA_ARGS__)
#define __ERROR(file, line, func, fmt, ...)  do {	\
	fputs(file ":" #line ":", stderr);	\
	fprintf(stderr, fmt, ##__VA_ARGS__);		\
	fputc('\n',stderr);				\
} while(0)

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

static FILE *serial_open(char const *fname)
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

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s <file> <motor a> <motor b>\n",
				argc?argv[0]:"hj");
		return -1;
	}

	FILE *sf = serial_open(argv[1]);
	if (!sf) {
		ERROR("open: %s", strerror(errno));
		return -1;
	}

	int motors[2];
	int ret = sscanf(argv[2], "%x", &motors[0]);
	if (ret != 1) {
		ERROR("not a number: \"%s\"", argv[1]);
		return -2;
	}

	ret = sscanf(argv[3], "%x", &motors[1]);
	if (ret != 1) {
		ERROR("not a number: \"%s\"", argv[2]);
		return -2;
	}

	char buf[1024];
	for(;;) {
		size_t len = frame_recv(sf, buf, sizeof(buf));
		struct hj_pktc_header *h = (typeof(h)) buf;

		if (len < 0) {
			fprintf(stderr, "frame_recv => %zu\n", len);
			continue;
		}

		switch(h->type) {
		case HJA_PT_TIMEOUT: {
			fprintf(stderr, "HJ_PT_TIMEOUT:");
			if (len != HJA_PL_TIMEOUT) {
				fprintf(stderr, "len = %zu, expected %d\n",
						len, HJA_PL_TIMEOUT);
				continue;
			}
			fputc('\n', stderr);
			send_req_info(sf);
			break;
		}
		case HJA_PT_INFO: {
			fprintf(stderr, "HJ_PT_INFO:");
			if (len != HJA_PL_INFO) {
				fprintf(stderr,	" len = %zu, expected %d\n",
						len, HJA_PL_INFO);
				continue;
			}
			struct hja_pkt_info *inf = (typeof(inf)) buf;
			fprintf(stderr, "\n\ta: ");
			print_hj_motor_info(&inf->a, stderr);
			fprintf(stderr, "\n\tb: ");
			print_hj_motor_info(&inf->b, stderr);
			fprintf(stderr, "\n");

			struct hjb_pkt_set_speed ss =
				HJB_PKT_SET_SPEED_INITIALIZER(motors[0],
						motors[1]);

			frame_send(sf, &ss, HJB_PL_SET_SPEED);

			break;
		}
		default: {
			fprintf(stderr, "recieved unknown pt %x, len %zu\n",
					h->type, len );
			break;
		}
		}
	}
	return 0;
}
