#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define print_i(fmt, ...) printf("%s:"fmt, __func__, ##__VA_ARGS__);
#define print_e(fmt, ...) printf("<error>%s:"fmt, __func__, ##__VA_ARGS__);

static int client_sfd = -1;
static struct sockaddr_in client_addr = {0, 0, {0x0}};
static struct sockaddr_in server_addr = {AF_INET, 0xE407, {0x0}};
static int ADDRLEN = sizeof(struct sockaddr_in);

#define MAX_MSG_SIZE (1024*1024)

int main(int argc, char *argv[])
{
	int ret = -1;
	char *send_buf = NULL;
	char *recv_buf = NULL;
	struct timeval tv = {60, 0};
	fd_set rfds;

	client_sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_sfd == -1) {
		print_e("socket failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}

	ret = connect(client_sfd, (struct sockaddr *)&server_addr, ADDRLEN);
	if (ret == -1) {
		print_e("connect failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}
	print_i("succeed to connect server.\n");
	ret = getsockname(client_sfd, (struct sockaddr *)&client_addr, &ADDRLEN);
	if (ret == -1) {
		print_e("getsockname failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}
	print_i("client_sfd=%d, client_addr(.sin_family=%d, .sin_port=%d, .sin_addr=0x%x).\n",
			client_sfd, client_addr.sin_family, ntohs(client_addr.sin_port), client_addr.sin_addr.s_addr);

	FD_ZERO(&rfds);
	FD_SET(0, &rfds);//stdin
	FD_SET(client_sfd, &rfds);//client
	while (1) {
		ret = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
		if (ret == -1) {
			print_e("select failed, errno{%d:%s}.\n", errno, strerror(errno));
		} else if (ret) {
			if (FD_ISSET(0, &rfds)) {//stdin
				memset(send_buf, 0, MAX_MSG_SIZE);
				read(0, send_buf, MAX_MSG_SIZE);
				ret = send(client_sfd, send_buf, strlen(send_buf), 0);
				if (ret == -1) {
					print_e("send failed, errno{%d:%s}.\n", errno, strerror(errno));
				}
			} else {//client
				if (FD_ISSET(client_sfd, &rfds)) {
					memset(recv_buf, 0, MAX_MSG_SIZE);
					ret = recv(client_sfd, recv_buf, MAX_MSG_SIZE, 0);
					if (ret == 0) {
						print_i("server port closed, let client close too.");
						close(client_sfd);
						client_sfd = -1;
						break;
					} else {
						print_i("%s\n", recv_buf);
					}
				}
			}
		} else {
			print_i("No data in 60s, errno{%d:%s}.\n", errno, strerror(errno));
		}
	}

	return 0;
}
