#include "common.h"

static struct tcp_listen_port msg_listen_port;
static int msg_listen_in_func(struct tcp_listen_port *port, char *buf, int count)
{
	int ret = -1;
	int index = 0;
	int len = 0;
	char tmp_buf[MAX_MSG_LEN];

	if (strncmp("exit\n", buf, strlen("exit\n")) == 0) {
		port->exit = 1;
	} else if (strcmp("list client\n", buf) == 0) {
		ret = 0;
		for (index=0; index<MAX_ACCEPT_NUMS; index++) {
			if (port->accept_sock[index].sfd >= 0) {
				len = sprintf(tmp_buf, "%d:%s\n", index,
						port->accept_sock[index].id);
				port->out_func(port, tmp_buf, len, -1);
			}
		}
		if (ret == 0) {
			len = sprintf(tmp_buf, "no client.\n");

			port->out_func(port, tmp_buf, len, -1);
		}
	} else if (strncmp(buf, "select ", strlen("select ")) == 0) {
		ret = sscanf(buf, "select %d", &port->default_index);
		if (ret != 1) errno_info("sscanf");
		if (port->default_index < 0 || port->default_index > MAX_ACCEPT_NUMS || port->accept_sock[port->default_index].sfd < 0) {
			len  = sprintf(tmp_buf, "please select right index.\n");
			port->out_func(port, tmp_buf, len, -1);
		}
	} else if (strncmp(buf, MSG_PREFIX, strlen(MSG_PREFIX)) == 0) {
		if (port->default_index >= 0 && port->default_index < MAX_ACCEPT_NUMS && port->accept_sock[port->default_index].sfd >= 0) {
			ret = poll_write(port->accept_sock[port->default_index].sfd, buf, count);

			ret = port->out_func(port, buf+2, count-2, port->default_index);//echo
		} else {
			len  = sprintf(tmp_buf, "please select right index.\n");
			port->out_func(port, tmp_buf, len, -1);
		}
	} else if (strncmp(buf, MSG_PREFIX, strlen(CMD_PREFIX)) == 0) {
		if (port->default_index >= 0 && port->default_index < MAX_ACCEPT_NUMS && port->accept_sock[port->default_index].sfd >= 0) {
			ret = poll_write(port->accept_sock[port->default_index].sfd, buf, count);

			ret = port->out_func(port, buf+2, count-2, port->default_index);//echo
		} else {
			len  = sprintf(tmp_buf, "please select right index.\n");
			port->out_func(port, tmp_buf, len, -1);
		}
	} else {
		len = sprintf(tmp_buf, "\n"
				"[list client/cmd]\tlist all client/cmd.\n"
				"[select\t\t\tindex] select client.\n"
				"[>>msg]\t\t\tsend msg.\n"
				"[>!cmd]\t\t\tsend cmd.\n"
				"[exit]\t\t\texit.\n"
		       );
		port->out_func(port, tmp_buf, len, -1);
	}

	return ret;
}
static int msg_listen_out_func(struct tcp_listen_port *port, char *buf, int count, int accept_index)
{
	int ret = -1;
	int index = 0;
	int len = 0;
	char tmp_buf[MAX_MSG_LEN*2];
	char dd[3] = {'>', '>', '\0'};
	int skip = 0;

	if (buf == NULL) return ret;
	buf[count] = '\0';

	if (strncmp(buf, MSG_PREFIX, strlen(MSG_PREFIX)) == 0) {
		dd[0] = '<';
		dd[1] = '<';
		skip = 2;
	} else if(strncmp(buf, CMD_PREFIX, strlen(CMD_PREFIX)) == 0) {
		dd[0] = '<';
		dd[1] = '!';
		skip = 2;
	}

	if (accept_index >=0 && port->accept_sock[accept_index].sfd >= 0)
		len = sprintf(tmp_buf, "%s %s %s:: %s", port->id, dd, port->accept_sock[accept_index].id, buf+skip);
	else
		len = sprintf(tmp_buf, "%s:: %s", port->id, buf+skip);
	
	ret = poll_write(port->out_fd, tmp_buf, len);

	return ret;
}

#define NORMAL_EXIT "normal exit"
static pthread_t pthread_msg_listen = 0;
static void *pthread_msg_listen_routine(void * arg)
{
	int ret = 0;
	char *res = NORMAL_EXIT;

	while (!msg_listen_port.exit) {
		ret = tcp_listen_port_recv_poll(&msg_listen_port);
		errno_info("poll");
		if (ret == -1) errno_info("poll")
		else {
			tcp_listen_port_accept(&msg_listen_port);
			tcp_listen_port_in(&msg_listen_port);
			tcp_listen_port_recv_dispense(&msg_listen_port);
		}
	}
	
	return res;
}

static void sig_handler(int signum)
{
	int len = 0;
	char tmp_buf[MAX_MSG_LEN];

	if (signum == SIGINT) {
		len  = sprintf(tmp_buf, "input 'exit' to quit.\n");
		msg_listen_port.out_func(&msg_listen_port, tmp_buf, len, -1);
	}
}

int main(int argc, char *argv[])
{
	int ret = -1;
	struct sockaddr_in addr;
	void *res = NULL;

	tcp_listen_port_initialize(&msg_listen_port);

	msg_listen_port.addr.sin_family = AF_INET;
	msg_listen_port.addr.sin_addr.s_addr = 0;
	msg_listen_port.addr.sin_port = bswap_16(2020);

	msg_listen_port.listen_sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (msg_listen_port.listen_sfd == -1) errno_return("socket");

	ret = bind(msg_listen_port.listen_sfd, (struct sockaddr *)&msg_listen_port.addr, ADDR_IN_LEN);
	if (ret == -1) errno_return("bind");

	ret = listen(msg_listen_port.listen_sfd, MAX_ACCEPT_NUMS);
	if (ret == -1) errno_return("listen");

	msg_listen_port.in_fd = 0;
	msg_listen_port.in_func = msg_listen_in_func;
	msg_listen_port.out_fd = 1;
	msg_listen_port.out_func = msg_listen_out_func;

	socket_ip_port(msg_listen_port.listen_sfd, msg_listen_port.id);
	log_i("msg port: %s\n", msg_listen_port.id);

	ret = pthread_create(&pthread_msg_listen, NULL, &pthread_msg_listen_routine, NULL);
	if (ret != 0) log_i("pthread_create failed<%d>.\n", ret);

	signal(SIGINT, sig_handler);

	ret = pthread_join(pthread_msg_listen, &res);
	if (ret != 0) log_i("pthread_join failed<%d>.\n", ret, strerror(errno));
	log_i("joined with thread 0x%lx(%s).\n", pthread_msg_listen, (char *)res);

	tcp_listen_port_clean(&msg_listen_port);

	return ret;
}
