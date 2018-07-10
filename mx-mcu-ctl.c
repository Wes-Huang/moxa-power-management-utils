#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "moxa-mcu.h"

static int connect_to_server(const char *sockname)
{
	int fd, len;
	struct sockaddr_un sock_addr;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "Open socket: %s\n", strerror(errno));
		return -1;
	}

	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, sockname);
	len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path);

	if (connect(fd, (struct sockaddr *) &sock_addr, len) < 0) {
		fprintf(stderr, "Connect socket: %s\n", strerror(errno));
		return -1;
	}

	return fd;
}

static int send_mcu_cmd(int fd, char read_buf[], char write_buf[], const int write_size)
{
	int ret, i;

	ret = write(fd, write_buf, write_size);
	if (ret < 0)
		return -1;

	memset(read_buf, 0, 16);
	ret = read(fd, read_buf, 16);
	if (ret < 0)
		return -1;
	return 0;
}

void usage(FILE *fp)
{
	fprintf(fp, "Usage:\n");
	fprintf(fp, "	mx-mcu-ctl <OPTIONS>\n");
	fprintf(fp, "OPTIONS:\n");
	fprintf(fp, "	-m, --mode [active|sleep|hibernate]\n");
	fprintf(fp, "		Set power management mode\n");
	fprintf(fp, "	-r, --red-led [on|off]\n");
	fprintf(fp, "		Set MCU red led\n");
	fprintf(fp, "	-g, --green-led [on|off]\n");
	fprintf(fp, "		Set MCU green led\n");
	fprintf(fp, "	-a, --a-version\n");
	fprintf(fp, "		Show MCU firmware a-version\n");
	fprintf(fp, "	-b, --b-version\n");
	fprintf(fp, "		Show MCU firmware b-version\n");
	fprintf(fp, "	-h, --help\n");
	fprintf(fp, "		Show the usage manual\n");
	fprintf(fp, "\n");
	fprintf(fp, "Example:\n");
	fprintf(fp, "	Set MCU to hibernate mode\n");
	fprintf(fp, "	# mx-mcu-ctl -m hiberbate\n");
	fprintf(fp, "\n");
	fprintf(fp, "	Turn on MCU red led\n");
	fprintf(fp, "	# mx-mcu-ctl -r on\n");
	fprintf(fp, "\n");
	fprintf(fp, "	Turn off MCU green led\n");
	fprintf(fp, "	# mx-mcu-ctl -g off\n");
}

int main(int argc, char *argv[])
{
	int i, fd, c;
	char read_buf[16];

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"mode", required_argument, 0, 'm'},
		{"red-led", required_argument, 0, 'r'},
		{"green-led", required_argument, 0, 'g'},
		{"a-version", no_argument, 0, 'a'},
		{"b-version", no_argument, 0, 'b'},
		{0, 0, 0, 0}
	};

	if (argc == 1 || argc > 3) {
		usage(stdout);
		return 99;
	}

	fd = connect_to_server(MCUD_SOCKET);
	if (fd < 0)
		return 1;

	while (1) {
		c = getopt_long(argc, argv, "hm:r:g:ab", long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			usage(stdout);
			exit(0);
		case 'm':
			if (!strncmp("active", optarg, 6)) {
				send_mcu_cmd(fd, read_buf, SET_ACTIVE_MODE, 5);
			} else if (!strncmp("sleep", optarg, 5)) {
				send_mcu_cmd(fd, read_buf, SET_SLEEP_MODE, 5);
			} else if (!strncmp("hibernate", optarg, 9)) {
				send_mcu_cmd(fd, read_buf, SET_HIBERNATE_MODE, 5);
			} else {
				usage(stderr);
				exit(99);			
			}
			break;
		case 'r':
			if (!strncmp("on", optarg, 2)) {
				send_mcu_cmd(fd, read_buf, SET_RED_LED_ON, 5);
			} else if (!strncmp("off", optarg, 3)) {
				send_mcu_cmd(fd, read_buf, SET_RED_LED_OFF, 5);
			} else {
				usage(stderr);
				exit(99);			
			}
			break;
		case 'g':
			if (!strncmp("on", optarg, 2)) {
				send_mcu_cmd(fd, read_buf, SET_GREEN_LED_ON, 5);
			} else if (!strncmp("off", optarg, 3)) {
				send_mcu_cmd(fd, read_buf, SET_GREEN_LED_OFF, 5);
			} else {
				usage(stderr);
				exit(99);			
			}
			break;
		case 'a':
				send_mcu_cmd(fd, read_buf, GET_MCU_A_VERSION, 4);
			break;
		case 'b':
				send_mcu_cmd(fd, read_buf, GET_MCU_B_VERSION, 4);
			break;
		default:
			usage(stderr);
			exit(99);
		}
	}

	for (i = 0; i < strlen(read_buf); i++)
		printf("%02x ", read_buf[i]);
	printf("\n");

	close(fd);
	return 0;
}
