#include <stdio.h>
#include <stdint.h>

#include <arpa/inet.h>

#include "frame_async.h"
#include "term_open.h"
#include "../hj_proto.h"

#include "hj_send.h"

int hj_send_pid_req(FILE *out)
{
	struct hj_pkt_header pr = HJB_PKT_PID_REQ_INITIALIZER;
	return frame_send(out, &pr, HJB_PL_PID_REQ);
}

int hj_send_req_info(FILE *out)
{
	struct hj_pkt_header ri = HJB_PKT_REQ_INFO_INITIALIZER;
	return frame_send(out, &ri, HJB_PL_REQ_INFO);
}

int hj_send_set_speed(FILE *sf, int16_t ml, int16_t mr)
{
	struct hjb_pkt_set_speed ss =
		HJB_PKT_SET_SPEED_INITIALIZER(ml, mr);
	return frame_send(sf, &ss, HJB_PL_SET_SPEED);
}

