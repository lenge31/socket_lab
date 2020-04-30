#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <signal.h>

#include "common.h"

static int client_sfd = -1;
static struct sockaddr_in client_addr = {0, 0, {0x0}};
struct sockaddr_in server_addr = {AF_INET, 0xE407, {0x0}};

#define MAX_MSG_SIZE (1024*1024)

static void sig_handler(int signum)
{
	//print_i("signum = %d.\n", signum);
	if (signum == SIGINT) {
		print_i("input 'exit' to quit.\n");
	}
}

int main(int argc, char *argv[])
{
	int ret = -1;
	char *send_buf = NULL;
	char *recv_buf = NULL;
	struct timeval tv;
	fd_set rfds;
	unsigned int port, addr_1, addr_2, addr_3, addr_4;

	signal(SIGINT, sig_handler);

	if (argc == 2) {
		sscanf(argv[1], "%d.%d.%d.%d:%d", &addr_1, &addr_2, &addr_3, &addr_4, &port);
		server_addr.sin_addr.s_addr = ((addr_4&0xff)<<24)|((addr_3&0xff)<<16)|((addr_2&0xff)<<8)|(addr_1&0xff);
		server_addr.sin_port = bswap_16(port);
	}

	dump_socketaddr_info("server", &server_addr);

	client_sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sfd == -1) errno_goto_out("socket");

	ret = connect(client_sfd, (struct sockaddr *)&server_addr, ADDRLEN);
	if (ret == -1) errno_goto_out("connect");

	dump_socket_info("client socket", client_sfd);

	send_buf = calloc(1, MAX_MSG_SIZE);
	if (send_buf == NULL) errno_goto_out("calloc send_buf");

	recv_buf = calloc(1, MAX_MSG_SIZE);
	if (recv_buf == NULL) errno_goto_out("calloc recv_buf");

	while (1) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);//stdin
		FD_SET(client_sfd, &rfds);//client

		ret = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
		if (ret == -1) {
			if (errno == EINTR) continue;
			errno_info("select");
		} else if (ret) {
			if (FD_ISSET(0, &rfds)) {//stdin
				memset(send_buf, 0, MAX_MSG_SIZE);
				read(0, send_buf, MAX_MSG_SIZE);
				if (strcmp("exit\n", send_buf) == 0) {
					break;//normally exit
				}

				if (send_buf[0] != '>') {
					print_i("stdin format:\n"
							"\tmessage {index>string}.\n"
					       );
					continue;
				}

				ret = send(client_sfd, send_buf+1, strlen(send_buf+1), 0);
				if (ret == -1) errno_info("send");
			} else {//client
				if (FD_ISSET(client_sfd, &rfds)) {
					memset(recv_buf, 0, MAX_MSG_SIZE);
					ret = recv(client_sfd, recv_buf, MAX_MSG_SIZE, 0);
					if (ret == 0) {
						print_i("server closed.\n");
						break;
					} else {
						print_i("<%s", recv_buf);
					}
				}
			}
		} else {//No data
		}
	}

out:
	if (send_buf != NULL) { free(send_buf); send_buf = NULL; }
	if (recv_buf != NULL) { free(recv_buf); recv_buf = NULL; }
	if (client_sfd >= 0) { close(client_sfd); client_sfd= -1; }

	return 0;
}
