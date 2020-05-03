#include "common.h"

//string space should be big enough.
void socket_ip_port(int sfd, char *string)
{
	struct sockaddr_in addr;
	int ret = 0;

	if (string == NULL) return;

	ret = getsockname(sfd, (struct sockaddr *)&addr, &ADDR_IN_LEN);
	if (ret == -1) { errno_info("getsockname") return; }

	sprintf(string, "%d.%d.%d.%d(%d)",
			addr.sin_addr.s_addr&0xff, addr.sin_addr.s_addr>>8&0xff,
			addr.sin_addr.s_addr>>16&0xff, addr.sin_addr.s_addr>>24&0xff,
			bswap_16(addr.sin_port)
	      );
}

//string space should be big enough.
void socketaddr_ip_port(struct sockaddr_in *addr, char *string)
{
	if (string == NULL) return;

	sprintf(string, "%d.%d.%d.%d(%d)",
			addr->sin_addr.s_addr&0xff, addr->sin_addr.s_addr>>8&0xff,
			addr->sin_addr.s_addr>>16&0xff, addr->sin_addr.s_addr>>24&0xff,
			bswap_16(addr->sin_port)
	      );
}

int listen_in_func_null(struct tcp_listen_port *port, char *buf, int count)
{
	return 0;
}
int listen_out_func_null(struct tcp_listen_port *port, char *buf, int count, int accept_index)
{
	return 0;
}

void tcp_listen_port_initialize(struct tcp_listen_port *port)
{
	int i = 0;

	if (port == NULL) return;

	memset(port, 0, sizeof(struct tcp_listen_port));
	port->listen_sfd = -1;
	port->in_fd = -1;
	port->in_func = listen_in_func_null;
	port->out_fd = -1;
	port->out_func = listen_out_func_null;
	for(i = 0; i < MAX_ACCEPT_NUMS; i++) {
		port->accept_sock[i].sfd = -1;
		port->accept_sock[i].send_fd = -1;
		port->accept_sock[i].recv_fd = -1;
	}
}

void tcp_listen_port_clean(struct tcp_listen_port *port)
{
	int i = 0;

	if (port == NULL) return;

	if (port->listen_sfd >= 0) close(port->listen_sfd);
	if (port->in_fd >= 0) close(port->in_fd);
	if (port->out_fd >= 0) close(port->out_fd);

	for(i = 0; i < MAX_ACCEPT_NUMS; i++) {
		if (port->accept_sock[i].sfd >= 0) close(port->accept_sock[i].sfd);
		if (port->accept_sock[i].send_fd >= 0) close(port->accept_sock[i].send_fd);
		if (port->accept_sock[i].recv_fd >= 0) close(port->accept_sock[i].recv_fd);
	}

	tcp_listen_port_initialize(port);
}

int tcp_listen_port_recv_poll(struct tcp_listen_port *port)
{
	int index = 0, ret = -1;

	if (port == NULL) return 0;

	port->poll_nfds = 0;
	if (port->listen_sfd >= 0) {
		port->poll_fds[port->poll_nfds].revents = 0;
		port->poll_fds[port->poll_nfds].events = POLLIN;
		port->poll_fds[port->poll_nfds++].fd = port->listen_sfd;
	}

	if (port->in_fd >= 0) {
		port->poll_fds[port->poll_nfds].revents = 0;
		port->poll_fds[port->poll_nfds].events = POLLIN;
		port->poll_fds[port->poll_nfds++].fd = port->in_fd;
	}

	for(index = 0; index < MAX_ACCEPT_NUMS; index++) {
		if (port->accept_sock[index].sfd >= 0) {
			port->poll_fds[port->poll_nfds].revents = 0;
			port->poll_fds[port->poll_nfds].events = POLLIN;
			port->poll_fds[port->poll_nfds++].fd = port->accept_sock[index].sfd;

			if (port->accept_sock[index].recv_fd >= 0) {
				port->poll_fds[port->poll_nfds].revents = 0;
				port->poll_fds[port->poll_nfds].events = POLLIN;
				port->poll_fds[port->poll_nfds++].fd = port->accept_sock[index].recv_fd;
			}
		}
	}

	ret = poll(port->poll_fds, port->poll_nfds, POLL_TIMEOUT);

	return ret;
}

int tcp_listen_port_accept(struct tcp_listen_port *port)
{
	int ret = -1;
	int i_fd = 0, index = 0;
	char send_buf[MAX_MSG_LEN];
	int count = 0;
	int sfd = -1;
	struct sockaddr_in addr;

	for (i_fd = 0; i_fd < port->poll_nfds; i_fd++)
		if ((port->poll_fds[i_fd].revents & POLLIN) != 0 && port->poll_fds[i_fd].fd == port->listen_sfd) break;

	if (i_fd >= port->poll_nfds) return ret;

	sfd = accept(port->listen_sfd, (struct sockaddr *)&addr, &ADDR_IN_LEN);
	if (sfd == -1) errno_info("accept")
	else {
		for (index=0; index<MAX_ACCEPT_NUMS; index++) {
			if (port->accept_sock[index].sfd < 0) {
				port->accept_sock[index].sfd = sfd;
				port->accept_sock[index].addr = addr;
				socketaddr_ip_port(&port->accept_sock[index].addr, port->accept_sock[index].id);

				count = sprintf(send_buf, "accept.\n");
				ret = port->out_func(port, send_buf, count, index);
				break;
			}
		}
		if (index >= MAX_ACCEPT_NUMS) {
			count = sprintf(send_buf, "reach to max connect, can't accept.\n");
			ret = port->out_func(port, send_buf, count, -1);
			ret = poll_write(sfd, send_buf, strlen(send_buf));
			close(sfd);
		}
		sfd = -1;
	}

	return ret;
}

int tcp_listen_port_in(struct tcp_listen_port *port)
{
	int ret = -1;
	int i_fd = 0;
	char recv_buf[MAX_MSG_LEN];
	int count = 0;
	struct sockaddr_in addr;

	for (i_fd = 0; i_fd < port->poll_nfds; i_fd++)
		if ((port->poll_fds[i_fd].revents & POLLIN) != 0 && port->poll_fds[i_fd].fd == port->in_fd) break;

	if (i_fd >= port->poll_nfds) return ret;

	count = read(port->in_fd, recv_buf, MAX_MSG_LEN);
	if (count == -1) errno_return("read")
	else {
		ret = port->in_func(port, recv_buf, count);
	}

	return ret;
}

int poll_write(int fd, char *buf, int count)
{
	int ret = 0;
	struct pollfd w_fd[1];

	if (fd < 0 && buf == NULL) return ret;

	w_fd[0].fd = fd;
	w_fd[0].revents = 0;
	w_fd[0].events = POLLOUT;

	ret = poll(w_fd, 1, POLL_TIMEOUT);
	if (ret == -1) errno_info("poll")
	else if (ret == 0) log_i("poll timeout, can't write.\n")
	else {
		ret = write(fd, buf, count);
		if (ret == -1) errno_info("write")
		else if (ret == 0) log_i("write timeout.\n");
	}

	return ret;
}

int tcp_listen_port_recv_dispense(struct tcp_listen_port *port)
{
	int i_fd = 0, index = 0;
	int ret = -1;
	char send_buf[MAX_MSG_LEN];
	char recv_buf[MAX_MSG_LEN];
	int fd = 0;
	int count = 0, len = 0;

	for (i_fd = 0; i_fd < port->poll_nfds; i_fd++) {
		if ((port->poll_fds[i_fd].revents & POLLIN) == 0) continue;

		for (index=0; index<MAX_ACCEPT_NUMS; index++) {
			if (port->accept_sock[index].sfd != port->poll_fds[i_fd].fd) continue;

			count = read(port->accept_sock[index].sfd, recv_buf, MAX_MSG_LEN);
			if (count == -1) errno_info("read")
			else if (count == 0) {
				len = sprintf(send_buf, "connect closed.\n");
				ret = port->out_func(port, send_buf, len, index);

				close(port->accept_sock[index].sfd);
				port->accept_sock[index].sfd = -1;

				fd = port->accept_sock[index].send_fd;
				if (fd >= 0) close(fd);
				port->accept_sock[index].send_fd = -1;

				fd = port->accept_sock[index].recv_fd;
				if (fd >= 0) close(fd);
				port->accept_sock[index].recv_fd = -1;
			} else {
				fd = port->accept_sock[index].send_fd;
				if (fd < 0)
					ret = port->out_func(port, recv_buf, count, index);
				else
					ret = poll_write(fd, recv_buf, count);
			}

			break;
		}
	}

	return ret;
}

int client_in_func_null(struct tcp_client_port *port, char *buf, int count)
{
	return 0;
}
int client_out_func_null(struct tcp_client_port *port, char *buf, int count, int to_server)
{
	return 0;
}

void tcp_client_port_initialize(struct tcp_client_port *port)
{
	if (port == NULL) return;

	memset(port, 0, sizeof(struct tcp_client_port));
	port->sfd = -1;
	port->in_func = client_in_func_null;
	port->in_fd = -1;
	port->out_fd = -1;
	port->out_func = client_out_func_null;
}

void tcp_client_port_clean(struct tcp_client_port *port)
{
	int i = 0;

	if (port == NULL) return;

	if (port->sfd >= 0) close(port->sfd);
	if (port->in_fd >= 0) close(port->in_fd);
	if (port->out_fd >= 0) close(port->out_fd);

	tcp_client_port_initialize(port);
}

int tcp_client_port_recv_poll(struct tcp_client_port *port)
{
	int ret = -1;

	if (port == NULL) return 0;

	port->poll_nfds = 0;
	if (port->sfd >= 0) {
		port->poll_fds[port->poll_nfds].revents = 0;
		port->poll_fds[port->poll_nfds].events = POLLIN;
		port->poll_fds[port->poll_nfds++].fd = port->sfd;
	}
	if (port->in_fd >= 0) {
		port->poll_fds[port->poll_nfds].revents = 0;
		port->poll_fds[port->poll_nfds].events = POLLIN;
		port->poll_fds[port->poll_nfds++].fd = port->in_fd;
	}


	ret = poll(port->poll_fds, port->poll_nfds, POLL_TIMEOUT);

	return ret;
}

int tcp_client_port_recv_handle(struct tcp_client_port *port)
{
	int i_fd = 0;
	int ret = -1;
	char recv_buf[MAX_MSG_LEN];
	int count = 0, len = 0;

	for (i_fd = 0; i_fd < port->poll_nfds; i_fd++)
		if ((port->poll_fds[i_fd].revents & POLLIN) != 0 && port->poll_fds[i_fd].fd == port->sfd) break;

	if (i_fd >= port->poll_nfds) return ret;

	count = read(port->sfd, recv_buf, MAX_MSG_LEN);
	if (count == -1) errno_info("read")
	else if (count == 0) {
		len = sprintf(recv_buf, "server closed.\n");
	}

	ret = port->out_func(port, recv_buf, len, 1);

	return ret;
}

int tcp_client_port_in(struct tcp_client_port *port)
{
	int ret = -1;
	int i_fd = 0;
	char recv_buf[MAX_MSG_LEN];
	int count = 0;
	struct sockaddr_in addr;

	for (i_fd = 0; i_fd < port->poll_nfds; i_fd++)
		if ((port->poll_fds[i_fd].revents & POLLIN) != 0 && port->poll_fds[i_fd].fd == port->in_fd) break;

	if (i_fd >= port->poll_nfds) return ret;

	count = read(port->in_fd, recv_buf, MAX_MSG_LEN);
	if (count == -1) errno_return("read")
	else {
		ret = port->in_func(port, recv_buf, count);
	}

	return ret;
}
