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

//#define print_i(fmt, ...) printf("%s:"fmt, __func__, ##__VA_ARGS__);
#define print_i printf
//#define print_e(fmt, ...) printf("<error>%s:"fmt, __func__, ##__VA_ARGS__);
#define print_e(fmt, ...) printf("<error>:"fmt, ##__VA_ARGS__);

static int client_sfd = -1;
static struct sockaddr_in client_addr = {0, 0, {0x0}};
struct sockaddr_in server_addr = {AF_INET, 0xE407, {0x0}};
static int ADDRLEN = sizeof(struct sockaddr_in);

#define MAX_MSG_SIZE (1024*1024)

int main(int argc, char *argv[])
{
	int ret = -1;
	char *send_buf = NULL;
	char *recv_buf = NULL;
	struct timeval tv;
	fd_set rfds;
	unsigned int port, addr_1, addr_2, addr_3, addr_4;

	if (argc == 2) {
		sscanf(argv[1], "%d.%d.%d.%d:%d", &addr_1, &addr_2, &addr_3, &addr_4, &port);
		server_addr.sin_addr.s_addr = ((addr_4&0xff)<<24)|((addr_3&0xff)<<16)|((addr_2&0xff)<<8)|(addr_1&0xff);
		server_addr.sin_port = bswap_16(port);
	}

	print_i("server_addr(.sin_family=%d, .sin_port=0x%x<%d>, .sin_addr=0x%x<%d.%d.%d.%d).\n",
			server_addr.sin_family, server_addr.sin_port, bswap_16(server_addr.sin_port), server_addr.sin_addr.s_addr, 
			server_addr.sin_addr.s_addr&0xff, server_addr.sin_addr.s_addr>>8&0xff,
			server_addr.sin_addr.s_addr>>16&0xff, server_addr.sin_addr.s_addr>>24&0xff);

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
	ret = getsockname(client_sfd, (struct sockaddr *)&client_addr, &ADDRLEN);
	if (ret == -1) {
		print_e("getsockname failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}
	print_i("client_sfd=%d, client_addr(.sin_family=%d, .sin_port=0x%x<%d>, .sin_addr=0x%x<%d.%d.%d.%d>).\n",
			client_sfd, client_addr.sin_family, client_addr.sin_port, bswap_16(client_addr.sin_port), client_addr.sin_addr.s_addr,
			client_addr.sin_addr.s_addr&0xff, client_addr.sin_addr.s_addr>>8&0xff,
			client_addr.sin_addr.s_addr>>16&0xff, client_addr.sin_addr.s_addr>>24&0xff);

	send_buf = calloc(1, MAX_MSG_SIZE);
	if (send_buf == NULL) {
		print_e("calloc send_buf failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}
	recv_buf = calloc(1, MAX_MSG_SIZE);
	if (recv_buf == NULL) {
		print_e("calloc recv_buf failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}

	while (1) {
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);//stdin
		FD_SET(client_sfd, &rfds);//client

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
						print_i("server closed.\n");
						close(client_sfd);
						client_sfd = -1;
						break;
					} else {
						print_i("%s", recv_buf);
					}
				}
			}
		} else {
			//print_i("No data, errno{%d:%s}.\n", errno, strerror(errno));
		}
	}

	free(send_buf);
	free(recv_buf);

	return 0;
}
