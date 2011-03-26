
#include <stdio.h>
#include "../hj_proto.h"


#define PS(tf, pt) printf(#pt ": %zu\n", (size_t)HJ##tf##_PL_##pt);

int main(void)
{
	PS(,MAX);
	PS(A,INFO);
	return 0;
}
