#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

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
			" cur_vel: %"PRIu16"",
			ntohs(inf->current),
			ntohl(inf->enc_ct),
			ntohs(inf->cur_vel));
}

int main(int argc, char **argv)
{
	char buf[1024];
	for(;;) {
		size_t len = frame_recv(stdin, buf, sizeof(buf));
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
			send_req_info(stdout);
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
