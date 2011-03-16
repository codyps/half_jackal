#include <stdint.h>
#include <stdbool.h>

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
	struct list_head list;
	size_t mem_len;
	size_t cur_pos;
	uint8_t data[];
} fcb_pkt;

#define pkt_from_list(list) container_of(list, struct fcb_pkt, l)

struct fcb_ctx_in {
	bool start;
	bool esc;
	uint16_t crc;
	struct fcb_pkt *cur_pkt;
	struct list_head pkts;
};

struct fcb_ctx_out {
	struct list_head pkts;
};

typedef struct fcb_ctx {
	int fd;
	struct fcb_ctx_out out;
	struct fcb_ctx_in in;
} fcb_ctx;

static void ctx_out_open(struct fcb_ctx_out *ci)
{
	list_head_init(&ci->pkts);
}

static void ctx_in_open(struct fcb_ctx_in *ci)
{
	ci->start = false;
	ci->esc = false;
	ci->crc = CRC_INIT;
	ci->cur_pkt = NULL;

	list_head_init(&ci->pkts);
}

/*
 * fcb_open - initalizes the given ctx with the given fd.
 *
 * @ctx - an opened ctx
 * @fd  - a file descriptor which supports poll, read, and write.
 *
 * return - failure <0, success 0.
 */
int fcb_open(struct fcb_ctx *ctx, int fd)
{
	ctx->fd = fd;

	ctx_in_open(&ctx->in);
	ctx_out_open(&ctx->out);

	return 0;
}

/*
 * clean_pkt - takes the raw packet `in_pkt` and extracts a single packet from
 *             it, starting processing at cur_pos. `in_pkt` is then modified
 *             with a new cur pos for where this processing left off.
 *
 *             If it finds that no new packets can be extracted, it
 *             reposistions the still raw data at the beginning of the buffer
 *             and fixes up the counts.
 *
 * @in_pkt: input packet with raw data populating `data`. Requires `cur_pos` be set
 *          to the posistion where parsing should begin.
 *
 * return:  when a new packet has been created: 0.
 *          when no more packets can be make and no packet has been made: 1.
 *          error: <0.
 */
static int clean_pkt(struct fcb_pkt *in_pkt, struct fcb_pkt **out_pkt)
{
	return -EINVAL;
}

/*
 * pkt_mk - allocate and intialize a fcb_pkt with data len = init_sz
 *
 * @init_sz - a suggestion for the size of the data[] in fcb_pkt.
 */
static fcb_pkt *pkt_mk(size_t init_sz)
{
	fcb_pkt *p = malloc(sizeof(*p) + init_sz);
	if (!p)
		return p;

	p->mem_len = init_sz;
	p->cur_pos = 0;

	list_head_init(&p->l);

	return p;
}

/*
 * in_update - called when the fd has data waiting to be 'read'. Attempts to
 *             read ths data, either continuing an in progress packet and/or
 *             beginning a new one. When a packet is completed, it should be
 *             decoded and added to the queue.
 *
 * @ctx:    the fcb_ctx to operate on
 *
 * return:  on error <0, success 0.
 */
static int in_update(fcb_ctx *ctx)
{
	struct fcb_ctx_in *ic = &ctx->in;

	/* do we currently have an in progress packet allocated? */
	if (!ic->cur_pkt) {
		ic->cur_pkt = pkt_mk(1024);
		if (!ic->cur_pkt)
			return -ENOMEM;
	}

	struct fcb_pkt *p = ic->cur_pkt;

	ssize_t ret = read(ctx->fd, p->data + p->cur_pos, p->mem_len - p->cur_pos);

	if (ret <= 0) {
		/* TODO: some error occured, handle. */
		return -1;
	}

	p->len += ret;

	/* Keep processing while data remains in cur_pkt. */
	for(;;) {
		struct fcb_pkt *np
		int r = clean_pkt(p, &np);

		if (r < 0)
			return r;
		if (r == 1) {
			/* no more data to process */
			break;
		}

		list_add(ic->pkts, np->list);
	}

	return 0;
}

/*
 * out_update - called when we know the fd has the ability to accept data via
 *              'write'. If we have any data in the output queue, it attempts
 *              to write a single packet (or remainder of a packet) prior to
 *              returning.
 *
 * @ctx - the fcb_ctx to operate on
 *
 * return - on error <0, on success 0, 1 when no data remains to be outputed.
 */
static int out_update(fcb_ctx *ctx)
{
	struct fcb_ctx_out *oc = &ctx->out;
	if (list_empty(oc->l))
		return 1;

	fcb_pkt *cur = list_first_entry(&oc->pkts, fcb_pkt, list);

	/* resume or begin writing out `cur' */

	/* FIXME: if pkts has only this element, do not attempt to write
	 * the embeded cancel byte (the last byte in data)
	 */
	ssize_t ret = write(ctx->fd, cur->data + cur->cur_pos,
				cur->len - cur->cur_pos);

	if (ret > 0) {
		cur->cur_pos += ret;
		if (cur->cur_pos == cur->len) {
			/* TODO: remove `cur` from the packet list,
			 * deallocate */
		}
	} else if (ret < 0) {
		/* TODO: some type of error, handle. */
		return -1;
	}

	return 0;
}

static int fcb_advance(struct fcb_ctx *ctx)
{
	return fcb_advance_wait(ctx, 0);
}

/*
 * fcb_advance_wait - determines (via poll) whether any data can be writen to
 *                    or read from the file descriptor, and then sends and
 *                    recives the maximum amount of data. Only blocking is done
 *                    up to timeout amount.
 *
 * @ctx:     the fcb_ctx to operate upon.
 * @timeout: amound to time to wait for poll to complete. If <1, this indicates
 *           an infinite wait. Infinite waits are assumed to mean the caller is
 *           waiting for all output packets to be processed.
 *
 * return - on error, < 1. On success 0.
 */
static int fcb_advance_wait(struct fcb_ctx *ctx, int timeout )
{
	struct pollfd pfd = { ctx.fd, POLLOUT | POLLIN, 0 };
	for (;;) {
		int r = poll(&pfd, 1, 0);
		if (r > 0) {
			/* we can do something */
			if (pfd.revents & POLLNVAL) {
				return -1;
			}

			if (pfd.revents & POLLERR) {
				return -2;
			}

			if (pfd.revents & POLLHUP) {
				return -3;
			}

			if (pfd.revents & POLLOUT) {
				/* do output update */
				int r = out_update(ctx);
				if (r == 1) {
					if (timeout < 0) {
						/* infinite timeout
						 * indicates we are waiting
						 * for the output to empty.
						 */
						return 0;
					} else {
						pfd.events &= ~POLLOUT;
					}
				} else if (r) {
					return r;
				}
			}

			if (pfd.revents & POLLIN) {
				/* do input update */
				/* FIXME: return value? */
				int r;
				if ((r = in_update(ctx))) {
					return r;
				}
			}
		} else if (r < 0) {
			/* error */
			return r;
		} else {
			/* can't do anything */
			return 0;
		}
	}
	return 0;
}

/*
 * pkt_append - given an additional byte, add it to the end of a packet.
 *
 * @p - the packet to append the byte to.
 * @b - the byte to append
 *
 * return - the new fcb_pkt on success, NULL on error (with errno set).
 */
static fcb_pkt *pkt_append(fcb_pkt *p, uint8_t b)
{
	if (p->cur_pos >= p->mem_len) {
		fcb_pkt *np = realloc(p, p->mem_len * 2);
		if (!np)
			return NULL;
		p = np;
	}

	p->data[p->cur_pos] = b;
	p->cur_pos ++;
	return p;
}

/*
 * PKT_ADD - try using pkt_append. On success, reassign the packet and return
 *           false.  on failure, return true without changing p
 */
#define PKT_ADD(p, c) ({		\
	bool fail = false;		\
	fcb_pkt *np = pkt_append(p, c);	\
	if (!np)			\
		fail = true;		\
	else				\
		(p) = np;		\
	fail; })

/*
 * PKT_ADD_B - append data bytes (which need to be escaped) and reasign @p
 *             properly.
 */
#define PKT_ADD_B(p, c) ({					\
	bool fail = false;					\
	if (FRAME_ESC_CHECK(c)) {				\
		if (PKT_ADD(p, FRAME_ESC)) {			\
			fail = true;				\
		} else {					\
			if (PKT_ADD(p, c ^ FRAME_ESC_MASK)) {	\
				fail = true;			\
			}					\
		}						\
	} else {						\
		if (PKT_ADD(p, c)) {				\
			fail = true;				\
		}						\
	}							\
	fail;	})

/*
 * convert_to_pkt - Take an outgoing data stream an make it a packet.
 *
 * @data   - raw data to form into the contents of a packet.
 * @nbytes - the length of @data.
 *
 * return  - a fcb_pkt (heap allocated) with the copied contents of @data.
 */
static fcb_pkt *convert_to_pkt(void *data, size_t nbytes)
{
	/* estimate the packet length. */
	size_t elen = 2 + nbytes + nbytes / (256 / 3);

	fcb_pkt *p = pkt_mk(elen);
	if (!p)
		return NULL;

	uint8_t *d;
	uint16_t crc = FRAME_CRC_INIT;

	if (PKT_ADD(p, FRAME_START)) {
		free(p);
		return NULL;
	}

	for (d = data; d < d + nbytes; d++) {
		uint8_t c = *d;
		if (FRAME_ESC_CHECK(c)) {
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

	if PKT_ADD_B(p, crc >> 8) {
		free(p);
		return NULL;
	}

	if PKT_ADD_B(p, crc & 0xff) {
		free(p);
		return NULL;
	}

	if (PKT_ADD(p, FRAME_START)) {
		free(p);
		return NULL;
	}

	if (PKT_ADD(p, FRAME_CANCEL)) {
		free(p);
		return NULL;
	}

	return p;
}

/*
 * fcb_send - send a packet.
 */
ssize_t fcb_send(struct fcb_ctx *ctx, void *data, size_t nbytes)
{
	fcb_pkt *pk = convert_to_pkt(data, nbytes);
	if (!pk)
		return -1;

	list_add(&pk->l, ctx->out.pkts);

	return fcb_advance(ctx);
}


/*
 * fcb_recv - get a packet. when no packet is avaliable, returns 0.
 *            otherwise, returns the length of the packet.
 *
 *            XXX: should this block? If it does, we need to be able to
 *            determine if a message is waiting for us.
 */
ssize_t fcb_recv(struct fcb_ctx *ctx, void *data, size_t nbytes)
{
	fcb_advance(ctx);
	/* if item exsists in queue, pop off.
	 * otherwise, try and process a recv. If it returns prior to completion,
	 * return -1?
	 */
	if (!list_empty(&ctx->in.l)) {
		struct list_head *ipl = ctx->in.l.next;

		list_del(ipl);

		struct fcb_pkt *pl = pkt_from_list(ipl);
		size_t pllen = pl->len;
		memcpy(data, pl->data, MIN(pl->len, nbytes));

		free(pl);

		return pllen;
	}

	/* no packet here. try again later. */
	return 0;
}

/*
 * fcb_flush - block until the output queue is empty.
 */
int fcb_flush(struct fcb_ctx *ctx)
{
	/* try to complete all outputs */
	return fcb_advance(ctx, -1);
}

