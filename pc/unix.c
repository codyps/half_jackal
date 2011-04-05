#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>

static char const *optstring = ":f:";

static char *lstrerror(int errnum)
{
	static char str[1024];
	strerror_r(errnum, str, sizeof(str));
	str[0] = tolower(str[0]);
	return str;
}

int main(int argc, char **argv)
{
	char *ufile = NULL;
	int c, errflg = 0;

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch(c) {
		case 'f':
			ufile = optarg;
			break;
		case ':':
			fprintf(stderr, "option -%c requires an operand\n",
					optopt);
			errflg++;
			break;
		case '?':
			fprintf(stderr, "unrecognized option -%c\n",
					optopt);
			errflg++;
			break;
		default:
			fprintf(stderr, "what\n");
			errflg++;
			break;
		}
	}

	if (errflg) {
		fputs("errors parsing args, exiting.\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (!ufile) {
		fputs("a filename (-f) is required\n", stderr);
		exit(EXIT_FAILURE);
	}

	int unix_socket = socket(AF_UNIX, SOCK_RDM, 0);

	if (unix_socket == -1) {
		fprintf(stderr, "not able to create socket: %s\n",
				lstrerror(errno));
	}

	return 0;
}
