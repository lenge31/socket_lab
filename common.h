#ifndef __COMMON_H__
#define __COMMON_H__

static int ADDRLEN = sizeof(struct sockaddr_in);
//#define print_i(fmt, ...) printf("%s:"fmt, __func__, ##__VA_ARGS__)
#define print_i(fmt, ...) printf(fmt, ##__VA_ARGS__)
//#define print_e(fmt, ...) printf("<error>%s:"fmt, __func__, ##__VA_ARGS__)
#define print_e(fmt, ...) printf("<error>:"fmt, ##__VA_ARGS__)

#define errno_return(what) \
{ \
	print_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno)); \
	return -errno; \
}
#define errno_goto_out(what) \
{ \
	print_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno)); \
	ret = -errno; \
	goto out; \
}
#define errno_info(what) \
{ \
	print_e("%s failed, errno{%d:%s}.\n", what, errno, strerror(errno)); \
	ret = -errno; \
}

static int dump_socket_info(char *what, int sfd)
{
	struct sockaddr_in addr;
	int ret = 0;

	ret = getsockname(sfd, (struct sockaddr *)&addr, &ADDRLEN);
	if (ret == -1) errno_return("getsockname");

	print_i("%s: sfd=%d, ip=%d.%d.%d.%d, port=%d.\n",
			what, sfd,
			addr.sin_addr.s_addr&0xff, addr.sin_addr.s_addr>>8&0xff, addr.sin_addr.s_addr>>16&0xff, addr.sin_addr.s_addr>>24&0xff,
			bswap_16(addr.sin_port)
	       );

	return 0;
}
static int dump_socketaddr_info(char *what, struct sockaddr_in *addr)
{
	int ret = 0;

	print_i("%s: ip=%d.%d.%d.%d, port=%d.\n",
			what,
			addr->sin_addr.s_addr&0xff, addr->sin_addr.s_addr>>8&0xff, addr->sin_addr.s_addr>>16&0xff, addr->sin_addr.s_addr>>24&0xff,
			bswap_16(addr->sin_port)
	       );

	return 0;
}

#endif
