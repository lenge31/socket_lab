#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <pthread.h>

//#define print_i(fmt, ...) printf("%s:"fmt, __func__, ##__VA_ARGS__)
#define print_i printf
//#define print_e(fmt, ...) printf("<error>%s:"fmt, __func__, ##__VA_ARGS__)
#define print_e(fmt, ...) printf("<error>:"fmt, ##__VA_ARGS__)

static int listen_sfd = -1;
struct sockaddr_in listen_addr = {AF_INET, 0xE407, {0x0}};
static int ADDRLEN = sizeof(struct sockaddr_in);
//struct sockaddr_in listen_addr = {AF_INET, 0xE407, {0x692AA8C0}};
#define MAX_CONNECT_COUNT 5
struct client_info {
	int accept_sfd;
	struct sockaddr_in accept_addr;
};
static struct client_info client_infos[MAX_CONNECT_COUNT];

static pthread_t pthread_id_client = 0;
#define MAX_MSG_SIZE (1024*1024)
#define ERR "abnormal_ret"
static void *pthread_routine_client(void *arg)
{
	int ret = -1, i = 0, j = 0, offset = 0;
	char *send_buf = NULL;
	char *recv_buf = NULL;
	struct timeval tv;
	fd_set rfds;
	int port = 0;

	print_i("start thread(0x%lx).\n", pthread_id_client);

	send_buf = calloc(1, MAX_MSG_SIZE);
	if (send_buf == NULL) {
		print_e("calloc send_buf failed, errno{%d:%s}.\n", errno, strerror(errno));
		return ERR;
	}
	recv_buf = calloc(1, MAX_MSG_SIZE);
	if (recv_buf == NULL) {
		print_e("calloc recv_buf failed, errno{%d:%s}.\n", errno, strerror(errno));
		return ERR;
	}

	while (1) {
		tv.tv_sec = 180;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);//stdin
		for (i=0; i<MAX_CONNECT_COUNT; i++) {
			if (client_infos[i].accept_sfd >= 0)
				FD_SET(client_infos[i].accept_sfd, &rfds);
		}

		ret = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
		if (ret == -1) {
			print_e("select failed, errno{%d:%s}.\n", errno, strerror(errno));
		} else if (ret) {
			if (FD_ISSET(0, &rfds)) {//stdin
				memset(send_buf, 0, MAX_MSG_SIZE);
				read(0, send_buf, MAX_MSG_SIZE);
				ret = sscanf(send_buf, "%d>", &port);
				if (ret != 1) {
					print_i("stdin format error. should be {port>string}.\n");
					for (i=0; i<MAX_CONNECT_COUNT; i++) {
						if (client_infos[i].accept_sfd != -1)
							print_i("index=%d,accept_sfd=%d, accept_addr(.sin_family=%d, .sin_port=%d, .sin_addr=0x%x)\n",
								i, client_infos[i].accept_sfd, client_infos[i].accept_addr.sin_family,
								ntohs(client_infos[i].accept_addr.sin_port), client_infos[i].accept_addr.sin_addr.s_addr);
					}
				} else {
					for (i=0; i<MAX_CONNECT_COUNT; i++) {
						if (client_infos[i].accept_sfd != -1 && ntohs(client_infos[i].accept_addr.sin_port) == port) break;
					}
					if (i >= MAX_CONNECT_COUNT) {
						print_e("it can't find port.\n");
					} else {
						for (offset=0; offset<MAX_MSG_SIZE; offset++)
							if (send_buf[offset] == '>') break;
						if (offset < MAX_MSG_SIZE) {
							offset++;
							ret = send(client_infos[i].accept_sfd, send_buf+offset, strlen(send_buf+offset), 0);
							if (ret == -1) {
								print_e("send failed, errno{%d:%s}.\n", errno, strerror(errno));
							}
						}
					}
				}
			} else {//client
				for (i=0; i<MAX_CONNECT_COUNT; i++) {
					if (FD_ISSET(client_infos[i].accept_sfd, &rfds)) {
						memset(recv_buf, 0, MAX_MSG_SIZE);
						ret = recv(client_infos[i].accept_sfd, recv_buf, MAX_MSG_SIZE, 0);
						if (ret == 0) {
							print_i("<%d>client port closed, let it close too.\n", ntohs(client_infos[i].accept_addr.sin_port));
							close(client_infos[i].accept_sfd);
							client_infos[i].accept_sfd = -1;
						} else {
							print_i("%d<%s", ntohs(client_infos[i].accept_addr.sin_port), recv_buf);
						}
					}
				}
			}
		} else {
			print_i("No data, errno{%d:%s}.\n", errno, strerror(errno));
		}
	}

	free(send_buf);
	free(recv_buf);

	return NULL;
}

int main(int argc, char *argv[])
{
	int ret = -1, i = 0;
	void *res = NULL;

	listen_sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sfd == -1) {
		print_e("socket failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	} else {
	}

	ret = bind(listen_sfd, (struct sockaddr *)&listen_addr, ADDRLEN);
	if (ret == -1) {
		print_e("bind failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}

	getsockname(listen_sfd, (struct sockaddr *)&listen_addr, &ADDRLEN);
	print_i("listen_sfd=%d, listen_addr(.sin_family=%d, .sin_port=%d, .sin_addr=0x%x)\n",
			listen_sfd, listen_addr.sin_family, ntohs(listen_addr.sin_port), listen_addr.sin_addr.s_addr);

	ret = listen(listen_sfd, MAX_CONNECT_COUNT);
	if (ret == -1) {
		print_e("listen failed, errno{%d:%s}.\n", errno, strerror(errno));
		return -errno;
	}

	memset(&client_infos, 0, sizeof(client_infos));
	for (i=0; i<MAX_CONNECT_COUNT; i++) {
		client_infos[i].accept_sfd = -1;
	}
	for (i=0; i<MAX_CONNECT_COUNT;) {
		if (client_infos[i].accept_sfd == -1) {
			client_infos[i].accept_sfd = accept(listen_sfd, (struct sockaddr *)&client_infos[i].accept_addr, &ADDRLEN);
			if (client_infos[i].accept_sfd == -1) {
				print_e("accept failed, errno{%d:%s}.\n", errno, strerror(errno));
			} else {
				print_i("index=%d, accept_sfd=%d, accept_addr(.sin_family=%d, .sin_port=%d, .sin_addr=0x%x)\n",
						i, client_infos[i].accept_sfd, client_infos[i].accept_addr.sin_family,
						ntohs(client_infos[i].accept_addr.sin_port), client_infos[i].accept_addr.sin_addr.s_addr);
				if (!pthread_id_client) {
					ret = pthread_create(&pthread_id_client, NULL, &pthread_routine_client, NULL);
					if (ret != 0) {
						print_e("pthread_create failed, errno{%d:%s}.\n", ret, strerror(ret));
						return -ret;
					}
				}
				i++;
			}
		}

		if (i >= MAX_CONNECT_COUNT) {//poll idle connect
			print_i("reach to max connect count, sleep 5s for waiting idle connect.\n");
			sleep(5);
			i = 0;
		}
	}

	ret = pthread_join(pthread_id_client, &res);
	if (ret != 0) {
		print_e("pthread_join failed, errno{%d:%s}.\n", ret, strerror(ret));
	}
	print_i("joined with thread 0x%lx; returned value was %s\n", pthread_id_client, (char *)res);
	pthread_id_client = 0;
	//free(res);

	return 0;
}
