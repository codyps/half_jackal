#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static char const *optstring = ":f:";

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

	int unix_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if (unix_socket == -1) {
		fprintf(stderr, "not able to create socket: %s\n",
				strerror(errno));
	}

	return 0;
}
