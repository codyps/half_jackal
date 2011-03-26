#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <arpa/inet.h>

#include "../hj_proto.h"

static void print_pktc_mi(struct hj_pktc_motor_info *inf, FILE *out)
{
	fprintf(out, "current: %"PRIu16" enc_p: %"PRIu32
			" enc_n: %"PRIu32" enc_l: %"PRIi16" pwr: %"PRIi16" vel: %"PRIi16,
			ntohs(inf->current),
			ntohl(inf->e.p),
			ntohl(inf->e.n),
			(int16_t)ntohs(inf->e.l),
			(int16_t)ntohs(inf->pwr),
			(int16_t)ntohs(inf->vel));
}
static void print_pktc_pk(struct hj_pktc_pid_k *pk, FILE *out)
{
	fprintf(out, "P: %"PRIi32" I: %"PRIi32" D: %"PRIi32" imax: %"PRIi16,
			(int32_t)ntohl(pk->p),
			(int32_t)ntohl(pk->i),
			(int32_t)ntohl(pk->d),
			(int16_t)ntohs(pk->i_max));
}

void hj_print_pid_k(struct hj_pkt_pid_k *inf, FILE *out)
{
	fputs("a: ", out);
	print_pktc_pk(&inf->k[0], out);
	fputs("\tb: ", out);
	print_pktc_pk(&inf->k[1], out);
}
void hj_print_info(struct hja_pkt_info *inf, FILE *out)
{
	fputs("a: ", out);
	print_pktc_mi(&inf->m[0], out);
	fputs("\tb: ", out);
	print_pktc_mi(&inf->m[1], out);
}

void hj_print_error(struct hja_pkt_error *e, FILE *out)
{
	char ver[sizeof(e->ver) + 1];
	char file[sizeof(e->file) + 1];

	strncpy(ver,  e->ver,  sizeof(e->ver));
	strncpy(file, e->file, sizeof(e->file));

	ver[sizeof(e->ver)] = 0;
	file[sizeof(e->file)] = 0;

	fprintf(stderr, "%s:%s:%"PRIu16" - %04"PRIx32"\n",
			ver,
			file,
			ntohs(e->line),
			ntohl(e->errnum));
}
