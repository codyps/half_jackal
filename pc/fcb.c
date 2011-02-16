
#if 0

struct frame_ctx {
	int fd;
	struct frame_ctx_i send, recv;
};

void frame_open(struct frame_ctx *fc, int fd)
{
	fc->fd = fd;
	fc->send.start = false;
	fc->send.esc = false;
	fc->send.crc = CRC_INIT;

	fc->recv.start = false;
	fc->recv.esc = false;
	fc->recv.crc = CRC_INIT;
}

ssize_t frame_send(struct frame_ctx *fc, void *data, size_t nbytes)
{
	if (fc->
	ssize_t x = write(fc->fd,
}

ssize_t frame_recv(struct frame_ctx *fc, void *vbuf, size_t nbytes)
{

}
#endif

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

struct frame_ctx_i {
	bool start;
	bool esc;
	uint16_t crc;
	struct list_head pkts;
};

struct fcb_pkt {
	struct list_head l;
	size_t mem_len;
	size_t cur_pos;
	uint8_t data[];
};

struct fcb_ctx {
	int fd;
	struct frame_ctx_i out;
	struct frame_ctx_i in;
};

static void new_packet(struct list_head pkts)
{

}

static void ctx_i_open(struct fcb_ctx_i *ci)
{
	ci->start = false;
	ci->esc = false;
	ci->crc = CRC_INIT;
}

int fcb_open(struct fcb_ctx *ctx, int fd)
{
	ctx->fd = fd;

	ctx_i_open(&ctx->in);
	ctx_i_open(&ctx->out);

	return 0;
}

ssize_t fcb_send(struct fcb_ctx *ctx, void *data, size_t nbytes)
{
	/* add item to end of list of items */
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

