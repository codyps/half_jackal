#ifndef HJ_PRINT_H_
#define HJ_PRINT_H_
#include <stdio.h>
#include "../hj_proto.h"
void hj_print_info(struct hja_pkt_info *inf, FILE *info);
void hj_print_error(struct hja_pkt_error *e, FILE *out);

#endif
