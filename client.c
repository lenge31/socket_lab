#include "common.h"

static struct tcp_client_port msg_client_port;

#define NORMAL_EXIT "normal exit"
static pthread_t pthread_msg_client = 0;
static void *pthread_msg_client_routine(void * arg)
{
	char *res = NORMAL_EXIT;
	int ret = 0;

	while (!msg_client_port.exit) {
		ret = tcp_client_port_recv_poll(&msg_client_port);
		errno_info("poll");
		if (ret == -1) errno_info("poll")
		else {
			tcp_client_port_in(&msg_client_port);
			tcp_client_port_recv_handle(&msg_client_port);
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
		msg_client_port.out_func(&msg_client_port, tmp_buf, len, 0);
	}
}

static int msg_client_in_func(struct tcp_client_port *port, char *buf, int count)
{
	int ret = -1;
	char tmp_buf[MAX_MSG_LEN];

	if (strncmp("exit\n", buf, strlen("exit\n")) == 0) {
		port->exit = 1;
	} else if (strcmp("list cmd\n", buf) == 0) {
	} else if (strncmp(buf, MSG_PREFIX, strlen(MSG_PREFIX)) == 0) {
		ret = poll_write(port->sfd, buf, count);

		ret = port->out_func(port, buf+2, count-2, 1);//echo
	} else if (strncmp(buf, CMD_PREFIX, strlen(CMD_PREFIX)) == 0) {
		ret = poll_write(port->sfd, buf, count);

		ret  = port->out_func(port, buf+2, count-2, 1);//echo
	} else {
		sprintf(tmp_buf,"\n" 
				"[list cmd]\tlist all cmd.\n",
				"[>>msg]\t\tsend msg.\n",
				"[>!cmd]\t\tsend cmd.\n"
		       );
		ret  = port->out_func(port, buf, count, 0);
	}

	return ret;
}
static int msg_client_out_func(struct tcp_client_port *port, char *buf, int count, int to_server)
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

	if (to_server)
		len = sprintf(tmp_buf, "%s %s %s:: %s", port->id, dd, port->server_id, buf+skip);
	else
		len = sprintf(tmp_buf, "%s:: %s", port->id, buf+skip);
	
	ret = poll_write(port->out_fd, tmp_buf, len);

	return ret;
}

int main(int argc, char *argv[])
{
	int ret = -1;
	int addr_1=0, addr_2=0, addr_3=0, addr_4=0, port=0;
	void *res = NULL;

	tcp_client_port_initialize(&msg_client_port);

	if (argc == 2) {
		sscanf(argv[1], "%d.%d.%d.%d:%d", &addr_1, &addr_2, &addr_3, &addr_4, &port);
		msg_client_port.server_addr.sin_addr.s_addr = ((addr_4&0xff)<<24)|((addr_3&0xff)<<16)|((addr_2&0xff)<<8)|(addr_1&0xff);
		msg_client_port.server_addr.sin_port = bswap_16(port);
	} else {
		msg_client_port.server_addr.sin_addr.s_addr = 0;
		msg_client_port.server_addr.sin_port = bswap_16(2020);
	}
	msg_client_port.server_addr.sin_family = AF_INET;
	socketaddr_ip_port(&msg_client_port.server_addr, msg_client_port.server_id);
	log_i("server is %s.\n", msg_client_port.server_id);

	msg_client_port.sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (msg_client_port.sfd == -1) errno_return("socket");

	msg_client_port.in_fd = 0;
	msg_client_port.in_func = msg_client_in_func;
	msg_client_port.out_fd = 1;
	msg_client_port.out_func = msg_client_out_func;

	ret = connect(msg_client_port.sfd, (struct sockaddr *)&msg_client_port.server_addr, ADDR_IN_LEN);
	if (ret == -1) errno_return("connect");

	socket_ip_port(msg_client_port.sfd, msg_client_port.id);
	log_i("client is %s.\n", msg_client_port.id);

	ret = pthread_create(&pthread_msg_client, NULL, &pthread_msg_client_routine, NULL);
	if (ret != 0) log_i("pthread_create failed<%d>.\n", ret);

	signal(SIGINT, sig_handler);

	ret = pthread_join(pthread_msg_client, &res);
	if (ret != 0) log_i("pthread_join failed<%d>.\n", ret, strerror(errno));
	log_i("joined with thread 0x%lx(%s).\n", pthread_msg_client, (char *)res);
	
	tcp_client_port_clean(&msg_client_port);

	return 0;
}
