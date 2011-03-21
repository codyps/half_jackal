#include <stdint.h>
#include <string.h>

#include "muc/muc.h"
#include "frame_async.h"
#include "../hj_proto.h"

#include "error_frame.h"

void __hj_send_error(uint16_t line, char *file, size_t flen,
		int32_t errnum)
{
	struct hja_pkt_error err_pkt = {
		.head = { .type = HJA_PT_ERROR },
		.errnum = htonl(errnum),
		.line = line
	};

	memcpy(err_pkt.file, file, MIN(flen, sizeof(err_pkt.file)));

	frame_send(&err_pkt, HJA_PL_ERROR);
}
