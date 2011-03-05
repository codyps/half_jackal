
#include <stdio.h>

#define ERROR(fmt, ...) _ERROR(__FILE__,__LINE__,__func__,fmt,##__VA_ARGS__)

#define _ERROR(file, line, func, fmt, ...) \
	__ERROR(file, line, func, fmt, ##__VA_ARGS__)
#define __ERROR(file, line, func, fmt, ...)  do {	\
	fputs(file ":" #line ":", stderr);	\
	fprintf(stderr, fmt, ##__VA_ARGS__);		\
	fputc('\n',stderr);				\
} while(0)
