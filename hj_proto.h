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
	int16_t vel_l;
	int16_t vel_r;
} __packed;

struct hj_pkt_req_info {
	struct hj_pktc_header head;
} __packed;

/** packets returned FROM the hj. **/
struct hj_pkt_info {
	struct hj_pktc_header head;
	struct hj_pktc_motor_info a, b;
} __packed;

struct hj_pkt_timeout {
	struct hj_pktc_header head;
} __packed;


/** **/

enum hj_pkt_len {
	HJ_PL_TIMEOUT = sizeof(struct hj_pkt_timeout),
	HJ_PL_SET_SPEED = sizeof(struct hj_pkt_set_speed),
	HJ_PL_INFO = sizeof(struct hj_pkt_info),
	HJ_PL_REQ_INFO = sizeof(struct hj_pkt_req_info)
};

#define HJ_PL_MIN (HJ_PL_TIMEOUT + 2)
#define HJ_PL_MAX (HJ_PL_INFO + 2)

enum hj_pkt_type {
	HJ_PT_TIMEOUT = 'a',
	HJ_PT_SET_SPEED,
	HJ_PT_REQ_INFO,
	HJ_PT_INFO
};

#define HJ_PKT_TIMEOUT_INITIALIZER { .head = { .type = HJ_PT_TIMEOUT } }
#define HJ_PKT_INFO_INITIALIZER { .head = { .type = HJ_PT_INFO } }
#define HJ_PKT_REQ_INFO_INITIALIZER { .head = { .type = HJ_PT_REQ_INFO } }

#endif
