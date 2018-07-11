#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "moxa-mcu.h"

struct mcu_struct {
	int mcu_fd;
	int server_fd;
	int running;
};

pthread_mutex_t mcu_mutex;

static int exec_cmd (char *ret_msg, char *cmd)
{
	FILE *fp;
	char command[MAXLINE];
	memset(command, '\0', MAXLINE);
	snprintf(command, sizeof(command), cmd);
	fp = popen(command, "r");
	if(NULL == fp) {
		perror("Cannot execute command \n");
		exit(1);
	}
	fgets(ret_msg, MAXLINE, fp);
	return pclose(fp);
}

static int init_tty(int fd)
{
	struct termios tmio;

	tcgetattr(fd, &tmio);
	/* set baudrate to 9600 */
	cfsetispeed(&tmio, B9600);
	cfsetospeed(&tmio, B9600);
	/* set databits to 8 */
	tmio.c_cflag = (tmio.c_cflag & ~CSIZE) | CS8;

	tmio.c_iflag = IGNBRK;
	tmio.c_lflag = 0;
	tmio.c_oflag = 0;
	tmio.c_cflag |= CLOCAL | CREAD;
	/* no flow control */
	tmio.c_cflag &= ~CRTSCTS;
	tmio.c_iflag &= ~(IXON|IXOFF|IXANY);

	tmio.c_cc[VMIN] = 1;
	tmio.c_cc[VTIME] = 5;
	/* no parity */
	tmio.c_cflag &= ~(PARENB | PARODD);
	/* set stopbits to 1 */
	tmio.c_cflag &= ~CSTOPB;

	tcflush(fd, TCIOFLUSH);
	return tcsetattr(fd, TCSANOW, &tmio);
}

static int open_tty(const char *ttyname)
{
	int fd;

	fd = open(ttyname, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		fprintf(stderr, "Open %s: %s\n", ttyname, strerror(errno));
		return -1;
	}

	if (init_tty(fd) < 0) {
		fprintf(stderr, "Set tty attributes: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

static int open_server(const char *sockname)
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

	unlink(sockname);
	len = sizeof(sock_addr.sun_family) + strlen(sock_addr.sun_path);

	if (bind(fd, (struct sockaddr *) &sock_addr, len) < 0) {
		fprintf(stderr, "Bind socket: %s\n", strerror(errno));
		return -1;
	}

	if (listen(fd, 1) < 0) {
		fprintf(stderr, "Socket listen: %s\n", strerror(errno));
		return -1;
	}

	return fd;
}

static void print_cmd(char buffer[], int len)
{
	int i;

	for (i = 0; i < len; i++)
		printf("%02x ", buffer[i]);
	printf("\n");
}

static int is_mcu_cmd_valid(char buffer[], int len)
{
	unsigned char chk_sum;
	int len_h, i;

	// The first byte must be 0xF0
	if (buffer[0] != '\xf0')
		return 0;

	// The second byte is the length of cmd & data
	len_h = (int) buffer[1];
	// Excluding the first byte, second byte, checksum
	if (len_h + 3 != len)
		return 0;

	// The last byte is checksum
	chk_sum = 0;
	for (i = 2; i < len - 1; i++)
		chk_sum += buffer[i];
	if (chk_sum != buffer[len - 1])
		return 0;

	// The byte after the last byte must be 0
	if (buffer[len] != 0)
		return 0;
	return 1;
}

/*
 * Return value:
 *	>0: success
 *	-1: read error
 *	-2: read timeout
 *	-3: identifier error
 *	-4: pattern error
 *	-5: nothing read
 */
static int read_mcu_cmd(int fd, char buffer[], int timeout)
{
	int ret, buf_offset, expected_len;
	time_t start_time, current_time;

	memset(buffer, 0, BUFF_LEN);
	buf_offset = 0;
	expected_len = BUFF_LEN;
	start_time = time(NULL);

	while (buf_offset < expected_len) {
		current_time = time(NULL);
		if (current_time - start_time > timeout)
			return E_TIMEOUT;

		ret = read(fd, &buffer[buf_offset], 1);
		if (ret < 0) {
			if (errno == EAGAIN) {
				if (buf_offset == 0)
					return E_AGAIN;
				continue;
			} else {
				perror("read");
				return E_SYSTEM_ERR;
			}
		}

		if (buf_offset == 0 && buffer[0] != '\xf0')
			return E_UNKNOWN_HEADER;
		else if (buf_offset == 1)
			expected_len = (int) buffer[1] + 3;
		buf_offset++;
	}

	if (!is_mcu_cmd_valid(buffer, buf_offset))
		return E_CMD_NOT_VALID;
	return buf_offset;
}

static int read_mcu_cmd_server(int fd, char buffer[], int timeout) {
	int ret;
	time_t start_time, current_time;

	start_time = time(NULL);
	while (1) {
		current_time = time(NULL);
		if (current_time - start_time > timeout)
			return E_TIMEOUT;

		ret = read_mcu_cmd(fd, buffer, TIMEOUT);
		if (ret != E_AGAIN)
			return ret;
	}
}

static void *server_thread(void *arg)
{
	struct mcu_struct *mcu = (struct mcu_struct *) arg;
	int ret, cli_fd, from_len;
	char buffer[BUFF_LEN];

	while (mcu->running) {
		// Accept connection from client
		cli_fd = accept(mcu->server_fd, NULL, &from_len);
		if (cli_fd < 0) {
			fprintf(stderr, "Socket accept: %s\n", strerror(errno));
			continue;
		}

		// Read from client
		memset(buffer, 0, BUFF_LEN);
		ret = read(cli_fd, buffer, BUFF_LEN);
		if (ret < 0) {
			write(cli_fd, "\xE2", 1);
			close(cli_fd);
			continue;
		}

		printf("Get from utility:\n");
		print_cmd(buffer, ret);

		if (!is_mcu_cmd_valid(buffer, ret)) {
			write(cli_fd, "\xE0", 1);
		} else {
			pthread_mutex_lock(&mcu_mutex);
			ret = write(mcu->mcu_fd, buffer, ret);
			if (ret < 0) {
				perror("write");
				pthread_mutex_unlock(&mcu_mutex);
				write(cli_fd, "\xE3", 1);
				close(cli_fd);
				continue;
			}

			ret = read_mcu_cmd_server(mcu->mcu_fd, buffer, TIMEOUT);
			if (ret < 0) {
				write(cli_fd, "\xE1", 1);
			} else {
				printf("Get from MCU:\n");
				print_cmd(buffer, ret);
				write(cli_fd, buffer, ret);				
			}

			pthread_mutex_unlock(&mcu_mutex);
		}
		close(cli_fd);
		printf("--------------------\n");
	}

	pthread_exit(NULL);
}

static void wait_event(struct mcu_struct *mcu)
{
	int ret;
	char buffer[BUFF_LEN];

	while (mcu->running) {
		pthread_mutex_lock(&mcu_mutex);
		ret = read_mcu_cmd(mcu->mcu_fd, buffer, TIMEOUT);
		pthread_mutex_unlock(&mcu_mutex);

		if (ret >= 0) {
			// Handle cmd
			printf("Interrupt!!!\n");
			print_cmd(buffer, ret);
			printf("--------------------\n");
			continue;
		}
	}

	return;
}

int main(int argc, char *argv[])
{
	pthread_t server_tid;
	struct mcu_struct mcu;

	// Initialize MCU port
	mcu.mcu_fd = open_tty(MCU_PORT);
	if (mcu.mcu_fd < 0)
		return 1;

	// Initialize server socket
	mcu.server_fd = open_server(MCUD_SOCKET);
	if (mcu.server_fd < 0)
		return 1;

	// Start running
	mcu.running = 1;
	pthread_create(&server_tid, NULL, server_thread, (void *) &mcu);
	wait_event(&mcu);

	// End
	close(mcu.server_fd);
	close(mcu.mcu_fd);
	return 0;
}
