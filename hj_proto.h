#ifndef HJ_PROTO_H_
#define HJ_PROTO_H_

#include <stdint.h>
#ifndef __packed
# define __packed __attribute__((packed))
#endif

/** packet components **/

/* sent at the start of each packet */
struct hj_pktc_header {
	uint8_t type;
} __packed;

struct hj_pktc_motor_info {
	uint16_t current;
	uint32_t enc_ct;
	uint16_t cur_vel;
} __packed;

/** packets dispatched TO the hj. **/
struct hj_pkt_set_speed {
	struct hj_pktc_header head;
	/* "vel" of both attached motors, TODO: determine units. */
	int16_t vel;
	uint16_t crc; /* the CCITT crc */
} __packed;

struct hj_pkt_req_info {
	struct hj_pktc_header head;
	uint16_t crc;
} __packed;

/** packets returned FROM the hj. **/
struct hj_pkt_info {
	struct hj_pktc_header head;
	struct hj_pktc_motor_info a, b;
	uint16_t crc;
} __packed;

struct hj_pkt_timeout {
	struct hj_pktc_header head;
	uint16_t crc;
} __packed;


/** **/

enum hj_pkt_len {
	HJ_PL_TIMEOUT = sizeof(struct hj_pkt_timeout),
	HJ_PL_SET_SPEED = sizeof(struct hj_pkt_set_speed),
	HJ_PL_INFO = sizeof(struct hj_pkt_info),
	HJ_PL_REQ_INFO = sizeof(struct hj_pkt_req_info)
};

#define HJ_PL_MIN HJ_PL_TIMEOUT
#define HJ_PL_MAX HJ_PL_INFO

enum hj_pkt_type {
	HJ_PT_TIMEOUT = 0,
	HJ_PT_SET_SPEED,
	HJ_PT_REQ_INFO,
	HJ_PT_INFO
};

#define HJ_PKT_TIMEOUT_INITIALIZER { .head = { .type = HJ_PT_TIMEOUT },\
	.crc = htons(0x1d0f) }

#endif
