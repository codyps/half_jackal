#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <signal.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>

static char const *optstring = ":f:";

struct conn {
	int fd;
};

static char *lstrerror(int errnum)
{
	static char str[1024];
	strerror_r(errnum, str, sizeof(str));
	str[0] = tolower(str[0]);
	return str;
}

static void term_handler(int sig, siginfo_t *inf, void *d)
{
	exit(3);
}

int main(int argc, char **argv)
{
	char *ufile = NULL;
	int c, errflg = 0;
	int ret;

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
		exit(1);
	}

	if (!ufile) {
		fputs("a filename (-f) is required\n", stderr);
		exit(1);
	}

	struct sigaction term_sa;
	term_sa.sa_flags = SA_SIGINFO;
	sigemptyset(&term_sa.sa_mask);
	term_sa.sa_sigaction = term_handler;

	ret = sigaction(SIGTERM, &term_sa, NULL);
	if (ret == -1) {
		fprintf(stderr, "registering term handler failed.");
		exit(2);
	}


	int unix_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (unix_socket == -1) {
		fprintf(stderr, "socket: %s\n",
				lstrerror(errno));
	}

	struct sockaddr_un sun;
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, ufile, sizeof(sun.sun_path));
	ret = bind(unix_socket, (struct sockaddr const*)&sun, sizeof(sun));
	if (ret == -1) {
		fprintf(stderr, "bind: %s\n",
				lstrerror(errno));
	}

	ret = listen(unix_socket, 0xf);
	if (ret == -1) {
		fprintf(stderr, "listen: %s\n",
				lstrerror(errno));
	}

	for(;;) {
		struct sockaddr_un sunp;
		socklen_t sl = sizeof(sunp);
		int s = accept(unix_socket, (struct sockaddr *)&sunp, &sl);
		if (s == -1) {
			fprintf(stderr, "accept: %s\n",
					lstrerror(errno));
			exit(2);
		}

		char *msg = "hello";
		write(s, msg, strlen(msg));

		close(s);
	}

	return 0;
}
