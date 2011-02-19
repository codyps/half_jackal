
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

struct list_head {
	struct list_head *prev, *next;
};

typedef struct fcb_pkt {
	struct list_head l;
	size_t mem_len;
	size_t cur_pos;
	uint8_t data[];
} fcb_pkt;

struct frame_ctx_in {
	bool start;
	bool esc;
	uint16_t crc;
	struct list_head pkts;
};

struct frame_ctx_out {
	bool start;
	struct list_head pkts;
};

static void ctx_out_open(struct fcb_ctx_out *ci)
{
	ci->start = false;
	ci->crc = CRC_INIT;
}

typedef struct fcb_ctx {
	int fd;
	struct frame_ctx_out out;
	struct frame_ctx_in in;
} fcb_ctx;

static void ctx_in_open(struct fcb_ctx_in *ci)
{
	ci->start = false;
	ci->esc = false;
	ci->crc = CRC_INIT;
}

int fcb_open(struct fcb_ctx *ctx, int fd)
{
	ctx->fd = fd;

	ctx_in_open(&ctx->in);
	ctx_out_open(&ctx->out);

	return 0;
}

/* attempt to do some writing of data, if possible */
ssize_t fcb_advance_out(fcb_ctx *ctx)
{

}

/* allocate and intialize a fcb_pkt with data len = init_sz */
static fcb_pkt *pkt_mk(size_t init_sz)
{
	fcb_pkt *p = malloc(sizeof(*p) + init_sz);
	if (!p)
		return p;

	p->mem_len = init_sz;
	p->cur_pos = 0;

	INIT_LIST_HEAD(&p->l);

	return p;
}

ssize_t fcb_send(struct fcb_ctx *ctx, void *data, size_t nbytes)
{
	/* add item to end of list of items */
	fcb_pkt *pk = malloc(sizeof(*pk) + nbytes);
	if (!pk) {
		return -1;
	}

	memcpy(pk->data, data, nbytes);

	list_add(&pk->l, ctx->out.pkts);

	return fcb_advance_out(ctx);
}

static fcb_pkt *pkt_append(fcb_pkt *p, uint8_t b)
{
	if (p->cur_pos >= p->mem_len) {
		fcb_pkt *np = realloc(p, p->mem_len * 2);
		if (!np)
			return NULL;
		p = np;
	}

	p->data[p->cur_pos] = b;
	return p;
}


#define PKT_ADD(p, c) ({		\
	bool fail = false;		\
	fcb_pkt *np = pkt_append(p, c);	\
	if (!np)			\
		fail = true;		\
	(p) = np;			\
	fail;
})

/* Take an outgoing data stream an make it a packet */
static fcb_pkt *convert_to_pkt(void *data, size_t nbytes)
{
	/* estimate the packet length. */
	size_t elen = 2 + nbytes + nbytes / (256 / 3);

	fcb_pkt *p = pkt_mk(elen);

	uint8_t *d;
	uint16_t crc = FRAME_CRC_INIT;
	for (d = data; d < d + nbytes; d++) {
		uint8_t c = *d;
		if (c == FRAME_START || c == FRAME_RESET || c == FRAME_ESC) {
			if PKT_ADD(p, FRAME_ESC) {
				free(p);
				return NULL;
			}
			c ^= FRAME_ESC_MASK;
		}

		crc = crc_ccitt_update(crc, c);
		if PKT_ADD(p, c) {
			free(p);
			return NULL;
		}
	}

	if PKT_ADD(p, crc >> 8) {
		free(p);
		return NULL;
	}

	if PKT_ADD(p, crc & 0xff) {
		free(p);
		return NULL;
	}

	return p;
}

ssize_t fcb_recv(struct fcb_ctx *ctx, void *data, size_t nbytes)
{
	/* if item exsists in queue, pop off.
	 * otherwise, try and process a recv. If it returns prior to completion,
	 * return -1?
	 */


}

int fcb_flush(struct fcb_ctx *ctx)
{
	/* try to complete all outputs */
}

