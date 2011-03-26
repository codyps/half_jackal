#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <errno.h>

#include <arpa/inet.h>

#include "frame_async.h"

#include "hj_send.h"
#include "hj_print.h"

#include "../hj_proto.h"

#include "term_open.h"
#include "error_m.h"

#define HJ_CASE(from_to, pkt_type)					\
	case HJ##from_to##_PT_##pkt_type:				\
		fputs("HJ" #from_to "_PT_" #pkt_type ":", stderr);	\
		if (len != HJ##from_to##_PL_##pkt_type) {		\
			fprintf(stderr, "len = %zu, expected %d\n",	\
				len, HJ##from_to##_PL_##pkt_type);	\
			continue;					\
		}

void hj_parse(FILE *sf, int16_t motors[2])
{
	char buf[1024];
	for(;;) {
		ssize_t len = frame_recv(sf, buf, sizeof(buf));
		struct hj_pkt_header *h = (typeof(h)) buf;

		if (len < 0) {
			fprintf(stderr, "frame_recv => %zd\n", len);
			exit(EXIT_FAILURE);
		}

		switch(h->type) {
		HJ_CASE(A, TIMEOUT) {
			fputc('\n', stderr);
			hj_send_req_info(sf);
			break;
		}

		HJ_CASE(A, INFO) {
			struct hja_pkt_info *inf = (typeof(inf)) buf;
			hj_print_info(inf, stderr);
			fputc('\n', stderr);
			hj_send_set_speed(sf, motors[0], motors[1]);
			break;
		}

		HJ_CASE(A, ERROR) {
			struct hja_pkt_error *e = (typeof(e)) buf;
			hj_print_error(e, stderr);
			break;
		}

		default: {
			fprintf(stderr, "recieved unknown pt %x, len %zu\n",
					h->type, len );
			break;
		}
		}
	}
}

static int16_t int16_or_die(char *in)
{
	int16_t num;
	int ret = sscanf(in, "%"SCNx16, &num);
	if (ret != 1) {
		ERROR("not a number: \"%s\"", in);
		exit(2);
	}
	return num;
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

	int16_t motors[2];
	motors[0] = int16_or_die(argv[2]);
	motors[1] = int16_or_die(argv[3]);

	hj_parse(sf, motors);

	return 0;
}
