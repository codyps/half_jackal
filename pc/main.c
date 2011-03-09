#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <errno.h>

#include <arpa/inet.h>

#include "frame_async.h"
#include "../hj_proto.h"

#include "term_open.h"
#include "error_m.h"

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
	fprintf(out, "current: %"PRIu16" enc_p: %"PRIu32
			" enc_n: %"PRIu32" pwr: %"PRIi16" vel: %"PRIi16,
			ntohs(inf->current),
			ntohl(inf->e.p),
			ntohl(inf->e.n),
			(int16_t)ntohs(inf->pwr),
			(int16_t)ntohs(inf->vel));
}

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s <file> <motor a> <motor b>\n",
				argc?argv[0]:"hj");
		return -1;
	}

	FILE *sf = term_open(argv[1]);
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
		ssize_t len = frame_recv(sf, buf, sizeof(buf));
		struct hj_pktc_header *h = (typeof(h)) buf;

		if (len < 0) {
			fprintf(stderr, "frame_recv => %zd\n", len);
			exit(EXIT_FAILURE);
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
			print_hj_motor_info(&inf->m[0], stderr);
			fprintf(stderr, "\n\tb: ");
			print_hj_motor_info(&inf->m[1], stderr);
			fprintf(stderr, "\n");

			struct hjb_pkt_set_speed ss =
				HJB_PKT_SET_SPEED_INITIALIZER(motors[0],
						motors[1]);

			frame_send(sf, &ss, HJB_PL_SET_SPEED);

			break;
		}
		case HJA_PT_ERROR: {
			fprintf(stderr, "HJ_PT_ERROR:");
			if (len != HJA_PL_ERROR) {
				fprintf(stderr, "len = %zu, expected %d\n",
						len, HJA_PL_ERROR);
				continue;
			}
			struct hja_pkt_error *e = (typeof(e)) buf;

			char file[sizeof(e->file) + 1];
			strncpy(file, e->file, sizeof(e->file));
			file[sizeof(e->file)] = 0;

			fprintf(stderr, "file: %s, line: %"
					PRIu16", errnum: %"PRIu8"\n",
				file, ntohs(e->line), ntohs(e->errnum));
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
