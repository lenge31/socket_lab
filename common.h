#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <byteswap.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <pthread.h>

#include <signal.h>

#ifndef __COMMON_H__
#define __COMMON_H__

#define MSG_PREFIX	">>"
#define CMD_PREFIX	">!"

static int ADDR_IN_LEN = sizeof(struct sockaddr_in);

#define MAX_ACCEPT_NUMS 2
#define MAX_MSG_LEN 256
#define POLL_TIMEOUT (3*1000)
struct accept_sock {
	int sfd;
	struct sockaddr_in addr;
	char id[MAX_MSG_LEN];
	int send_fd;
	int recv_fd;
};

struct tcp_listen_port {
	struct sockaddr_in addr;
	char id[MAX_MSG_LEN];
	int listen_sfd;

	int in_fd;
	int (*in_func)(struct tcp_listen_port *port, char *buf, int count);
	int out_fd;
	int (*out_func)(struct tcp_listen_port *port, char *buf, int count, int accept_index);

	struct accept_sock accept_sock[MAX_ACCEPT_NUMS];
	int default_index;

	struct pollfd poll_fds[3+(3*MAX_ACCEPT_NUMS)];
	int poll_nfds;

	int exit;
};

void tcp_listen_port_initialize(struct tcp_listen_port *port);
void tcp_listen_port_clean(struct tcp_listen_port *port);
int tcp_listen_port_recv_dispense(struct tcp_listen_port *port);
int poll_write(int fd, char *buf, int count);
int tcp_listen_port_in(struct tcp_listen_port *port);
int tcp_listen_port_accept(struct tcp_listen_port *port);
int tcp_listen_port_recv_poll(struct tcp_listen_port *port);

struct tcp_client_port {
	struct sockaddr_in addr;
	char id[MAX_MSG_LEN];
	int sfd;

	int in_fd;
	int (*in_func)(struct tcp_client_port *port, char *buf, int count);
	int out_fd;
	int (*out_func)(struct tcp_client_port *port, char *buf, int count, int to_server);

	struct sockaddr_in server_addr;
	char server_id[MAX_MSG_LEN];

	struct pollfd poll_fds[3];
	int poll_nfds;

	int exit;
};
void tcp_client_port_initialize(struct tcp_client_port *port);
void tcp_client_port_clean(struct tcp_client_port *port);
int tcp_client_port_in(struct tcp_client_port *port);
int tcp_client_port_recv_handle(struct tcp_client_port *port);
int tcp_client_port_recv_poll(struct tcp_client_port *port);

static int log_fd = 1;
#define log_i(fmt, ...)\
{\
	dprintf(log_fd, "<i>"fmt, ##__VA_ARGS__);\
}
#define log_e(fmt, ...)\
{\
	dprintf(log_fd, "<e>"fmt, ##__VA_ARGS__);\
}
#define log_pure(fmt, ...)\
{\
	dprintf(log_fd, fmt, ##__VA_ARGS__);\
}

#define errno_return(what)\
{\
	log_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno));\
	return -errno;\
}
#define errno_break(what)\
{\
	log_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno));\
	ret = -errno;\
	break;\
}
#define errno_goto_out(what)\
{\
	log_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno));\
	ret = -errno;\
	goto out;\
}
#define errno_info(what)\
{\
	log_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno));\
	ret = -errno;\
}

void socket_ip_port(int sfd, char *string);
void socketaddr_ip_port(struct sockaddr_in *addr, char *string);

#endif
