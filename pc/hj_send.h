#ifndef HJ_SEND_H_
#define HJ_SEND_H_
#include <stdio.h>
#include <stdint.h>

int hj_send_set_speed(FILE *sf, int16_t ml, int16_t mr);
int hj_send_req_info(FILE *out);

#endif
