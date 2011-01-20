#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
	struct hj_pkt_req_info ri = HJ_PKT_REQ_INFO_INITIALIZER;
	frame_send(out, &ri, HJ_PL_REQ_INFO);
}

int main(int argc, char **argv)
{
	send_req_info(stdout);
	return 1;
	char buf[1024];
	for(;;) {
		size_t len = frame_recv(stdin, buf, sizeof(buf));
		struct hj_pktc_header *h = (typeof(h)) buf;

		switch(h->type) {
		case HJ_PT_TIMEOUT: {
			fprintf(stderr, "recieved timeout packet\n");
			send_req_info(stdout);
			break;
		}
		default: {
			fprintf(stderr, "recieved unknown pt %x, len %zu\n",
					h->type, len );
			struct hj_pkt_req_info ri = HJ_PKT_REQ_INFO_INITIALIZER;
			frame_send(stdout, &ri, HJ_PL_REQ_INFO);
			break;
		}
		}
	}
	return 0;
}
