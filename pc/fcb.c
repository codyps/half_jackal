
#if 0
struct frame_ctx_i {
	bool start;
	bool esc;
	uint16_t crc;
};

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

struct list_head {

};

struct fcb_ctx {
	int fd;
	struct list_head out_list;
	struct list_head in_list;
};

#define FRAME_INIT_SZ 1024
int fcb_open(struct fcb_ctx *ctx, int fd)
{
	ctx->fd = fd;
	ctx->buf = malloc(FRAME_INIT_SZ);
	if (!ctx->buf) {
		return -1;
	}
	ctx->mem_sz = FRAME_INIT_SZ;
	ctx->ct = 0;

	return 0;
}

ssize_t fcb_send(struct fcb_ctx *ctx, void *data, size_t nbytes)
{
}

