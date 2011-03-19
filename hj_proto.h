#ifndef HJ_PROTO_H_
#define HJ_PROTO_H_

#include <stdint.h>
#ifndef __packed
# define __packed __attribute__((packed))
#endif

/* naming notes:
 * hj : prefix from project name
 * hja: from the avr
 * hjb: from the controlling thing, to the avr
 */

/** packet components **/

/* sent at the start of each packet, and sometimes sent by itself. */
struct hj_pkt_header {
	uint8_t type;
} __packed;

struct hj_pktc_enc {
	uint32_t p;
	uint32_t n;
} __packed;

struct hj_pktc_motor_info {
	uint16_t current;
	struct hj_pktc_enc e;
	int16_t pwr;
	int16_t vel;
} __packed;

struct hj_pktc_pid_k {
	int32_t p;
	int32_t i;
	int32_t d;
	int16_t i_max;
} __packed;

/** packets dispatched both ways **/
struct hj_pkt_pid_k {
	struct hj_pkt_header head;
	struct hj_pktc_pid_k k[2];
} __packed;

/** packets dispatched TO the hj. **/
struct hjb_pkt_set_speed {
	struct hj_pkt_header head;
	/* "vel" of both attached motors, TODO: determine units. */
#define HJ_MOTOR_L 0
#define HJ_MOTOR_R 1
	int16_t vel[2];
} __packed;

/** packets returned FROM the hj. **/
struct hja_pkt_info {
	struct hj_pkt_header head;
	struct hj_pktc_motor_info m[2];
} __packed;

struct hja_pkt_error {
	struct hj_pkt_header head;
	uint8_t errnum;
	uint16_t line;
	char file[6];
} __packed;

/** **/
union hj_pkt_union {
	struct hj_pkt_header a;
	struct hja_pkt_info b;
	struct hja_pkt_error c;
	struct hjb_pkt_set_speed d;
	struct hj_pkt_pid_k e;
};

enum hj_pkt_len {
	HJ_PL_HEADER = sizeof(struct hj_pkt_header),

	HJA_PL_TIMEOUT  = HJ_PL_HEADER,
	HJB_PL_REQ_INFO = HJ_PL_HEADER,
	HJB_PL_PID_REQ  = HJ_PL_HEADER,
	HJB_PL_PID_SAVE = HJ_PL_HEADER,

	HJA_PL_INFO = sizeof(struct hja_pkt_info),
	HJA_PL_ERROR = sizeof(struct hja_pkt_error),
	HJB_PL_SET_SPEED = sizeof(struct hjb_pkt_set_speed),
	HJ_PL_PID_K = sizeof(struct hj_pkt_pid_k),

	HJ_PL_MIN = sizeof(struct hj_pkt_header),
	HJ_PL_MAX = sizeof(union hj_pkt_union)
};

enum hj_pkt_type {
	HJA_PT_TIMEOUT = 'a',
	HJA_PT_INFO,
	HJA_PT_ERROR,
	HJB_PT_SET_SPEED,
	HJB_PT_REQ_INFO,
	HJB_PT_PID_SAVE,
	HJB_PT_PID_REQ,

	HJ_PT_PID_K
};

#define HJB_PKT_REQ_INFO_INITIALIZER { .type = HJB_PT_REQ_INFO }
#define HJA_PKT_TIMEOUT_INITIALIZER  { .type = HJA_PT_TIMEOUT  }

#define HJ_PKT_PID_K_INITIALIZER { .head = { .type = HJ_PT_PID_K } }
#define HJA_PKT_INFO_INITIALIZER { .head = { .type = HJA_PT_INFO } }

#define HJA_PKT_ERROR_INITIALIZER(err) { .head = { .type = HJA_PT_ERROR }, \
	.line = htons(__LINE__), .file = __FILE__, .errnum = htons(err) }
#define HJB_PKT_SET_SPEED_INITIALIZER(a,b)		\
	{ .head = { .type = HJB_PT_SET_SPEED},		\
		.vel = { htons(a), htons(b) } }

#endif
