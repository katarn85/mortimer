/*
 *  DA probe
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 *
 * Hyunjong Park <phjwithyou.park@samsung.com>
 * Juyoung Kim <j0.kim@samsung.com>
 * Anastasia Lyupa <a.lyupa@samsung.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contributors:
 * - S-Core Co., Ltd
 * - Samsung RnD Institute Russia
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/un.h>
#include <errno.h>
#include <time.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include "probeinfo.h"
#include "dautil.h"
#include "da_socket.h"
#include "daprobe.h"
#include "dahelper.h"
#include "api_id_mapping.h"
#include "binproto.h"
#include "real_functions.h"

#define OBJ_DUMMY 0

static enum DaOptions _sopt = OPT_NETWORK;
void getAddress(const struct sockaddr *sa, char *address) {
	char buff[INET6_ADDRSTRLEN];
	char *path;

	if (sa == NULL) {
		const char sa_is_null[] = "<addr is NULL>";
		memcpy(address, sa_is_null, sizeof(sa_is_null));
		return;
	}

	switch (sa->sa_family) {
	case AF_INET:
		snprintf(address, MAX_PATH_LENGTH, "%s:%d",
			 inet_ntoa(((struct sockaddr_in*) sa)->sin_addr),
			 (int) ntohs(((struct sockaddr_in*) sa)->sin_port));
		break;
	case AF_INET6:
		snprintf(address, MAX_PATH_LENGTH, "%s:%d",
			 inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) sa)->sin6_addr),
			 buff, sizeof(buff)),
				ntohs(((struct sockaddr_in6*) sa)->sin6_port));
		break;
	case AF_UNIX:
		path = ((struct sockaddr_un*)sa)->sun_path;
		if (path[0] == '\0')
			snprintf(address, MAX_PATH_LENGTH, "%s", path + 1);
		else
			snprintf(address, MAX_PATH_LENGTH, "%s", path);
		break;
	default:
		snprintf(address, MAX_PATH_LENGTH, "<unknown socket type #%d>",
			 sa->sa_family);
		break;
	}
}

//FD
int socket(int domain, int type, int protocol) {
	static int (*socketp)(int domain, int type, int protocol);
	BEFORE_ORIGINAL_SOCK(socket, LIBC);
	ret = socketp(domain, type, protocol);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, ret, SOCKET_API_FD_OPEN, info,
				 "ddd", domain, type, protocol);

	return ret;
}

int accept(int socket, struct sockaddr *address, socklen_t *address_len) {

	static int (*acceptp)(int socket, struct sockaddr *address,
			socklen_t *address_len);

	BEFORE_ORIGINAL_SOCK(accept, LIBC);

	char callAddress[MAX_PATH_LENGTH];
	getAddress(address, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, socket,
						 SOCKET_API_ACCEPT_START, info,
						 "dsp", socket, callAddress,
						 voidp_to_uint64(address_len));

	ret = acceptp(socket, address, address_len);
	info.sock = (struct sockaddr *)address;

	getAddress(address, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, socket,
					       SOCKET_API_ACCEPT_END, info,
					       "dsp", socket, callAddress,
					       voidp_to_uint64(address_len));

	return ret;
}

int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
	static int (*accept4p)(int sockfd, struct sockaddr *addr,
			socklen_t *addrlen, int flags);

	BEFORE_ORIGINAL_SOCK(accept4, LIBC);

	char callAddress[MAX_PATH_LENGTH];
	getAddress(addr, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, sockfd,
						 SOCKET_API_ACCEPT_START, info,
						 "dspd", sockfd, callAddress,
						 voidp_to_uint64(addrlen),
						 flags);

	ret = accept4p(sockfd, addr, addrlen, flags);
	info.sock = (struct sockaddr *)addr;

	getAddress(addr, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, sockfd,
					       SOCKET_API_ACCEPT_END, info,
					       "dspd", sockfd, callAddress,
					       voidp_to_uint64(addrlen), flags);

	return ret;
}

int connect(int socket, const struct sockaddr *address, socklen_t address_len) {
	static int (*connectp)(int socket, const struct sockaddr *address,
			socklen_t address_len);

	BEFORE_ORIGINAL_SOCK(connect, LIBC);

	ret = connectp(socket, address, address_len);
	info.sock = (struct sockaddr *)address;

	char callAddress[MAX_PATH_LENGTH];
	getAddress(address, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket, SOCKET_API_CONNECT,
			info, "dsd",
			socket, callAddress, address_len);

	return ret;
}

int shutdown(int socket, int how) {
	static int (*shutdownp)(int socket, int how);

	BEFORE_ORIGINAL_SOCK(shutdown, LIBC);

	ret = shutdownp(socket, how);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket, SOCKET_API_FD_CLOSE,
			info, "dd", socket, how);

	return ret;
}

int bind(int socket, const struct sockaddr *address, socklen_t address_len) {
	static int (*bindp)(int socket, const struct sockaddr *address,
			socklen_t address_len);

	BEFORE_ORIGINAL_SOCK(bind, LIBC);

	ret = bindp(socket, address, address_len);
	info.sock = (struct sockaddr *)address;

	char callAddress[MAX_PATH_LENGTH];
	getAddress(address, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket, SOCKET_API_BIND,
			info, "dsd", socket, callAddress, address_len);

	return ret;
}

int listen(int socket, int backlog) {
	static int (*listenp)(int socket, int backlog);
	BEFORE_ORIGINAL_SOCK(listen, LIBC);
	ret = listenp(socket, backlog);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket, SOCKET_API_LISTEN,
				 info, "dd", socket, backlog);

	return ret;
}

ssize_t send(int socket, const void *message, size_t length, int flags) {
	static ssize_t (*sendp)(int socket, const void *message, size_t length,
			int flags);
	ssize_t sret, result;
	BEFORE_ORIGINAL_SOCK(send, LIBC);

	char *messagP = (char *)message;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('x', NULL, OBJ_DUMMY, socket,
						  SOCKET_API_SEND_START, info,
						  "dpdd", socket,
						  voidp_to_uint64(messagP),
						  length, flags);

	sret = sendp(socket, message, length, flags);
	result = sret;
	if (result < 0)
		result = 0;
	info.msg_total_size = result;
	info.msg_pack_size = result>SOCKET_SEND_SIZE?SOCKET_SEND_SIZE:result;
	info.msg_buf = messagP;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('x', sret, OBJ_DUMMY, socket,
					       SOCKET_API_SEND_END, info,
					       "dpdd", socket,
					       voidp_to_uint64(messagP),
					       length, flags);
	return sret;
}

ssize_t recv(int socket, void *buffer, size_t length, int flags) {
	static ssize_t (*recvp)(int socket, void *buffer, size_t length, int flags);
	ssize_t sret, result;

	BEFORE_ORIGINAL_SOCK(recv, LIBC);

	char *bufferP = (char *)buffer;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('x', NULL, OBJ_DUMMY, socket,
						 SOCKET_API_RECV_START, info,
						 "dpdd", socket,
						 voidp_to_uint64(bufferP),
						 length, flags);

	sret = recvp(socket, buffer, length, flags);
	result = sret;
	if (result < 0)
		result = 0;
	info.msg_total_size = result;
	info.msg_pack_size = result>SOCKET_SEND_SIZE?SOCKET_SEND_SIZE:result;
	info.msg_buf = bufferP;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('x', sret, OBJ_DUMMY, socket,
					       SOCKET_API_RECV_END, info,
					       "dpdd", socket,
					       voidp_to_uint64(bufferP),
					       length, flags);
	return sret;
}

ssize_t sendto(int socket, const void *message, size_t length, int flags,
		const struct sockaddr *dest_addr, socklen_t dest_len) {
	static ssize_t (*sendtop)(int socket, const void *message, size_t length,
			int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
	ssize_t sret, result;

	BEFORE_ORIGINAL_SOCK(sendto, LIBC);

	char *bufferP = (char *)message;
	info.sock = (struct sockaddr *) dest_addr;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('x', NULL, OBJ_DUMMY, socket,
						 SOCKET_API_SEND_START, info,
						 "dpddpd", socket,
						 voidp_to_uint64(bufferP),
						 length, flags,
						 voidp_to_uint64(dest_addr),
						 dest_len);

	sret = sendtop(socket, message, length, flags, dest_addr, dest_len);
	result = sret;
	if (result < 0)
		result = 0;
	info.msg_total_size = result;
	info.msg_pack_size = result>SOCKET_SEND_SIZE?SOCKET_SEND_SIZE:result;
	info.msg_buf = bufferP;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('x', sret, OBJ_DUMMY, socket,
					       SOCKET_API_SEND_END, info,
					       "dpddpd", socket,
					       voidp_to_uint64(bufferP),
					       length, flags,
					       voidp_to_uint64(dest_addr), dest_len);
	return sret;
}

ssize_t recvfrom(int socket, void *buffer, size_t length, int flags,
		struct sockaddr *address, socklen_t *address_len) {
	static ssize_t (*recvfromp)(int socket, void *buffer, size_t length,
			int flags, struct sockaddr *address, socklen_t *address_len);
	ssize_t sret, result;

	BEFORE_ORIGINAL_SOCK(recvfrom, LIBC);

	char *bufferP = (char *)buffer;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('x', NULL, OBJ_DUMMY, socket,
						 SOCKET_API_RECV_START, info,
						 "dpddpp", socket,
						 voidp_to_uint64(bufferP),
						 length, flags,
						 voidp_to_uint64(address),
						 voidp_to_uint64(address_len));

	sret = recvfromp(socket, buffer, length, flags, address, address_len);
	info.sock = (struct sockaddr *)address;

	result = sret;
	if (result < 0)
		result = 0;
	info.msg_total_size = result;
	info.msg_pack_size = result>SOCKET_SEND_SIZE?SOCKET_SEND_SIZE:result;
	info.msg_buf = bufferP;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('x', sret, OBJ_DUMMY, socket,
					       SOCKET_API_RECV_END, info,
					       "dpddpp", socket,
					       voidp_to_uint64(bufferP),
					       length, flags,
					       voidp_to_uint64(address),
					       voidp_to_uint64(address_len));
	return sret;
}

ssize_t recvmsg(int socket, struct msghdr *message, int flags) {
	static ssize_t (*recvmsgp)(int socket, struct msghdr *message, int flags);
	ssize_t sret;

	BEFORE_ORIGINAL_SOCK(recvmsg, LIBC);

	int* bufferP = (int*) message->msg_name;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('x', NULL, OBJ_DUMMY, socket,
						 SOCKET_API_RECV_START, info,
						 "dpd", socket,
						 voidp_to_uint64(message),
						 flags);

	sret = recvmsgp(socket, message, flags);
	if (sret <= 0) {
		memset(&message, 0, sizeof(message));
	}

	int sendMaxSize = SOCKET_SEND_SIZE;
	char* out = (char*) real_malloc(sendMaxSize + 5);
	if (sret <= 0) {
		out[0] = '\0';
	} else {
		memcpy(out, message, sendMaxSize + 5);
		if (sret > sendMaxSize + 5) {
			out[sendMaxSize] = '.';
			out[sendMaxSize + 1] = '.';
			out[sendMaxSize + 2] = '.';
			out[sendMaxSize + 3] = '\0';
		} else {
			out[sret] = '\0';
		}
	}

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('x', sret, OBJ_DUMMY, socket,
					       SOCKET_API_RECV_END, info, "dpd",
					       socket, voidp_to_uint64(bufferP),
					       flags);
	free(out);
	return sret;
}

ssize_t sendmsg(int socket, const struct msghdr *message, int flags) {
	static ssize_t (*sendmsgp)(int socket, const struct msghdr *message,
			int flags);
	ssize_t sret;

	BEFORE_ORIGINAL_SOCK(sendmsg, LIBC);

	int* bufferP = (int*) message->msg_name;

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('x', NULL, OBJ_DUMMY, socket,
						 SOCKET_API_SEND_START, info,
						 "dpd", socket,
						 voidp_to_uint64(message),
						 flags);

	sret = sendmsgp(socket, message, flags);
	if (sret <= 0) {
		memset(&message, 0, sizeof(message));
	}
	int sendMaxSize = SOCKET_SEND_SIZE;
	char* out = (char*) real_malloc(sendMaxSize + 5);
	if (sret <= 0) {
		out[0] = '\0';
	} else {
		memcpy(out, message, sendMaxSize + 5);
		if (sret > sendMaxSize + 5) {
			out[sendMaxSize] = '.';
			out[sendMaxSize + 1] = '.';
			out[sendMaxSize + 2] = '.';
			out[sendMaxSize + 3] = '\0';
		} else {
			out[sret] = '\0';
		}
	}

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('x', sret, OBJ_DUMMY, socket,
					       SOCKET_API_SEND_END, info, "dpd",
					       socket, voidp_to_uint64(bufferP),
					       flags);
	free(out);
	return sret;
}

int getsockopt(int socket, int level, int option_name, void *option_value,
		socklen_t *option_len) {
	static int (*getsockoptp)(int socket, int level, int option_name,
			void *option_value, socklen_t *option_len);

	BEFORE_ORIGINAL_SOCK(getsockopt, LIBC);

	ret = getsockoptp(socket, level, option_name, option_value, option_len);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket, SOCKET_API_OTHER,
				 info, "dddpp", socket, level, option_name,
				 voidp_to_uint64(option_value),
				 voidp_to_uint64(option_len));

	return ret;
}

int setsockopt(int socket, int level, int option_name, const void *option_value,
		socklen_t option_len) {
	static int (*setsockoptp)(int socket, int level, int option_name,
			const void *option_value, socklen_t option_len);

	BEFORE_ORIGINAL_SOCK(setsockopt, LIBC);

	ret = setsockoptp(socket, level, option_name, option_value, option_len);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket, SOCKET_API_OTHER,
				 info, "dddpd", socket, level, option_name,
				 voidp_to_uint64(option_value), option_len);

	return ret;
}

int getpeername(int fd, struct sockaddr *addr, socklen_t *len) {
	static int (*getpeernamep)(int s, struct sockaddr *addr, socklen_t *len);

	BEFORE_ORIGINAL_SOCK(getpeername, LIBC);

	ret = getpeernamep(fd, addr, len);
	info.sock = (struct sockaddr *)addr;

	char callAddress[MAX_PATH_LENGTH];
	getAddress(addr, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, fd, SOCKET_API_OTHER,
				 info, "dsp", fd, callAddress,
				 voidp_to_uint64(len));
	return ret;
}

int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	static int (*getsocknamep)(int sockfd, struct sockaddr *addr,
			socklen_t *addrlen);

	BEFORE_ORIGINAL_SOCK(getsockname, LIBC);

	ret = getsocknamep(sockfd, addr, addrlen);
	info.sock = (struct sockaddr *)addr;

	char callAddress[MAX_PATH_LENGTH];
	getAddress(addr, callAddress);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, sockfd, SOCKET_API_OTHER,
				 info, "dsp", sockfd, callAddress,
				 voidp_to_uint64(addrlen));

	return ret;
}

int socketpair(int domain, int type, int protocol, int socket_vector[2]) {
	static int (*socketpairp)(int domain, int type, int protocol,
			int socket_vector[2]);

	BEFORE_ORIGINAL_SOCK(socketpair, LIBC);

	ret = socketpairp(domain, type, protocol, socket_vector);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, socket_vector[0],
			SOCKET_API_FD_OPEN, info, "ddddd",
			domain, type, protocol, socket_vector[0], socket_vector[1]);

	return ret;
}

int sockatmark(int __fd) {
	static int (*sockatmarkp)(int __fd);

	BEFORE_ORIGINAL_SOCK(sockatmark, LIBC);

	ret = sockatmarkp(__fd);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, __fd,
			SOCKET_API_OTHER, info, "d", __fd);
	return ret;
}

int isfdtype(int __fd, int __fdtype) {
	static int (*isfdtypep)(int __fd, int __fdtype);

	BEFORE_ORIGINAL_SOCK(isfdtype, LIBC);

	ret = isfdtypep(__fd, __fdtype);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, __fd, SOCKET_API_OTHER, info,
				 "dd", __fd, __fdtype);
	return ret;
}

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		struct timeval *timeout) {
	static int (*selectp)(int nfds, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, struct timeval *timeout);

	BEFORE_ORIGINAL_SOCK(select, LIBC);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, 0,
						 SOCKET_API_EVENT_START, info,
						 "dpppp", nfds,
						 voidp_to_uint64(readfds),
						 voidp_to_uint64(writefds),
						 voidp_to_uint64(exceptfds),
						 voidp_to_uint64(timeout));

	ret = selectp(nfds, readfds, writefds, exceptfds, timeout);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, 0,
					       SOCKET_API_EVENT_END, info,
					       "dpppp", nfds,
					       voidp_to_uint64(readfds),
					       voidp_to_uint64(writefds),
					       voidp_to_uint64(exceptfds),
					       voidp_to_uint64(timeout));

	return ret;
}

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
		const struct timespec *ntimeout, const sigset_t *sigmask) {
	static int (*pselectp)(int nfds, fd_set *readfds, fd_set *writefds,
			fd_set *exceptfds, const struct timespec *ntimeout,
			const sigset_t *sigmask);

	BEFORE_ORIGINAL_SOCK(pselect, LIBC);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, 0,
						 SOCKET_API_EVENT_START, info,
						 "dppppp",nfds,
						 voidp_to_uint64(readfds),
						 voidp_to_uint64(writefds),
						 voidp_to_uint64(exceptfds),
						 voidp_to_uint64(ntimeout),
						 voidp_to_uint64(sigmask));

	ret = pselectp(nfds, readfds, writefds, exceptfds, ntimeout, sigmask);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, 0,
					       SOCKET_API_EVENT_END, info,
					       "dppppp", nfds,
					       voidp_to_uint64(readfds),
					       voidp_to_uint64(writefds),
					       voidp_to_uint64(exceptfds),
					       voidp_to_uint64(ntimeout),
					       voidp_to_uint64(sigmask));

	return ret;
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
	static int (*pollp)(struct pollfd *ufds, unsigned int nfds, int timeout);

	BEFORE_ORIGINAL_SOCK(poll, LIBC);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, fds->fd,
						 SOCKET_API_EVENT_START, info,
						 "pxd", voidp_to_uint64(fds),
						 (uint64_t)(nfds), timeout);

	ret = pollp(fds, nfds, timeout);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, fds->fd,
					       SOCKET_API_EVENT_END, info,
					       "pxd", voidp_to_uint64(fds),
					       (uint64_t)(nfds), timeout);

	return ret;
}

int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout_ts,
		const sigset_t *sigmask) {
	static int (*ppollp)(struct pollfd *fds, nfds_t nfds,
			const struct timespec *timeout_ts, const sigset_t *sigmask);

	BEFORE_ORIGINAL_SOCK(ppoll, LIBC);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, fds->fd,
						 SOCKET_API_EVENT_START, info,
						 "pxpp",
						 voidp_to_uint64(fds),
						 (uint64_t)(nfds),
						 voidp_to_uint64(timeout_ts),
						 voidp_to_uint64(sigmask));

	ret = ppollp(fds, nfds, timeout_ts, sigmask);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, fds->fd,
					       SOCKET_API_EVENT_END, info,
					       "pxpp",
					       voidp_to_uint64(fds),
					       (uint64_t)(nfds),
					       voidp_to_uint64(timeout_ts),
					       voidp_to_uint64(sigmask));

	return ret;
}

int epoll_create(int __size) {
	static int (*epoll_createp)(int __size);
	BEFORE_ORIGINAL_SOCK(epoll_create, LIBC);

	ret = epoll_createp(__size);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, ret, SOCKET_API_FD_OPEN, info,
				 "d", __size);
	return ret;
}

int epoll_create1(int __flags) {
	static int (*epoll_create1p)(int __size);
	BEFORE_ORIGINAL_SOCK(epoll_create1, LIBC);

	ret = epoll_create1p(__flags);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, ret, SOCKET_API_FD_OPEN, info,
				 "d", __flags);
	return ret;
}

int epoll_wait(int __epfd, struct epoll_event *__events, int __maxevents,
		int __timeout) {
	static int (*epoll_waitp)(int __epfd, struct epoll_event *__events,
			int __maxevents, int __timeout);
	BEFORE_ORIGINAL_SOCK(epoll_wait, LIBC);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, __epfd,
						 SOCKET_API_EVENT_START, info,
						 "dpdd", __epfd,
						 voidp_to_uint64(__events),
						 __maxevents, __timeout);

	ret = epoll_waitp(__epfd, __events, __maxevents, __timeout);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, __epfd,
					       SOCKET_API_EVENT_END, info,
					       "dpdd", __epfd,
					       voidp_to_uint64(__events),
					       __maxevents, __timeout);

	return ret;
}

int epoll_pwait(int __epfd, struct epoll_event *__events, int __maxevents,
		int __timeout, __const                          __sigset_t *__ss) {
	static int (*epoll_pwaitp)(int __epfd, struct epoll_event *__events,
			int __maxevents, int __timeout, __const                          __sigset_t *__ss);
	BEFORE_ORIGINAL_SOCK(epoll_pwait, LIBC);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_START('d', NULL, OBJ_DUMMY, __epfd,
						 SOCKET_API_EVENT_START, info,
						 "dpddp", __epfd,
						 voidp_to_uint64(__events),
						 __maxevents, __timeout,
						 voidp_to_uint64(__ss));

	ret = epoll_pwaitp(__epfd, __events, __maxevents, __timeout, __ss);

	AFTER_ORIGINAL_LIBC_SOCK_WAIT_FUNC_END('d', ret, OBJ_DUMMY, __epfd,
					       SOCKET_API_EVENT_END, info,
					       "dpddp", __epfd,
					       voidp_to_uint64(__events),
					       __maxevents, __timeout,
					       voidp_to_uint64(__ss));

	return ret;
}

int epoll_ctl(int __epfd, int __op, int __fd, struct epoll_event *__event) {
	static int (*epoll_ctlp)(int __epfd, int __op, int __fd,
			struct epoll_event *__event);

	BEFORE_ORIGINAL_SOCK(epoll_ctl, LIBC);

	ret = epoll_ctlp(__epfd, __op, __fd, __event);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, __fd, SOCKET_API_OTHER,
				 info, "dddp", __epfd, __op, __fd,
				 voidp_to_uint64(__event));
	return ret;
}

#if 0
//OPTION _//NO FD

uint32_t htonl(uint32_t hostlong) {
	static uint32_t (*htonlp)(uint32_t hostlong);
	uint32_t uret;

	BEFORE_ORIGINAL_SOCK(htonl, LIBC);

	uret = htonlp(hostlong);

	AFTER_ORIGINAL_LIBC_SOCK('d', uret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", hostlong);

	return uret;
}

uint16_t htons(uint16_t hostshort) {
	static uint16_t (*htonsp)(uint16_t hostshort);
	uint16_t uret;

	BEFORE_ORIGINAL_SOCK(htons, LIBC);

	uret = htonsp(hostshort);

	AFTER_ORIGINAL_LIBC_SOCK('d', (uint32_t)uret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", hostshort);

	return uret;
}

int inet_aton(const char *cp, struct in_addr *inp) {
	static int (*inet_atonp)(const char *cp, struct in_addr *inp);

	BEFORE_ORIGINAL_SOCK(inet_aton, LIBC);

	ret = inet_atonp(cp, inp);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pp", voidp_to_uint64(cp),
				 voidp_to_uint64(inp));

	return ret;
}

in_addr_t inet_addr(const char *cp) {
	static in_addr_t (*inet_addrp)(const char *cp);
	in_addr_t iret;

	BEFORE_ORIGINAL_SOCK(inet_addr, LIBC);

	iret = inet_addrp(cp);

	AFTER_ORIGINAL_LIBC_SOCK(iret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(cp));

	return iret;
}

in_addr_t inet_network(const char *cp) {
	static in_addr_t (*inet_networkp)(const char *cp);
	in_addr_t iret;

	BEFORE_ORIGINAL_SOCK(inet_network, LIBC);

	iret = inet_networkp(cp);

	AFTER_ORIGINAL_LIBC_SOCK(iret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(cp));

	return iret;
}

char *inet_ntoa(struct in_addr in) {
	static char * (*inet_ntoap)(struct in_addr in);
	char* sret;

	BEFORE_ORIGINAL_SOCK(inet_ntoa, LIBC);

	sret = inet_ntoap(in);

	AFTER_ORIGINAL_LIBC_SOCK(sret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "d", in.s_addr);

	return sret;
}

uint32_t ntohl(uint32_t netlong) {
	static uint32_t (*ntohlp)(uint32_t netlong);
	uint32_t uret;

	BEFORE_ORIGINAL_SOCK(ntohl, LIBC);

	uret = ntohlp(netlong);

	AFTER_ORIGINAL_LIBC_SOCK('d', uret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "d", netlong);

	return uret;
}

uint16_t ntohs(uint16_t netshort) {
	static uint16_t (*ntohsp)(uint16_t netshort);
	uint16_t uret;

	BEFORE_ORIGINAL_SOCK(ntohs, LIBC);

	uret = ntohsp(netshort);

	AFTER_ORIGINAL_LIBC_SOCK('d', (uint32_t)uret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "d", netshort);

	return uret;
}

in_addr_t inet_lnaof(struct in_addr in) {
	static in_addr_t (*inet_lnaofp)(struct in_addr in);
	in_addr_t iret;

	BEFORE_ORIGINAL_SOCK(inet_lnaof, LIBC);

	iret = inet_lnaofp(in);

	AFTER_ORIGINAL_LIBC_SOCK('d', iret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "d", in.s_addr);

	return iret;
}

in_addr_t inet_netof(struct in_addr in) {
	static in_addr_t (*inet_netofp)(struct in_addr in);
	in_addr_t iret;

	BEFORE_ORIGINAL_SOCK(inet_netof, LIBC);

	iret = inet_netofp(in);

	AFTER_ORIGINAL_LIBC_SOCK('d', iret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "d", in.s_addr);

	return iret;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
	static const char* (*inet_ntopp)(int af, const void *src, char *dst,
			socklen_t size);
	const char* cret;

	BEFORE_ORIGINAL_SOCK(inet_ntop, LIBC);

	cret = inet_ntopp(af, src, dst, size);

	AFTER_ORIGINAL_LIBC_SOCK(cret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dppd", af, voidp_to_uint64(src), voidp_to_uint64(dst), size);

	return cret;
}

int inet_pton(int af, const char *src, void *dst) {
	static int (*inet_ptonp)(int af, const char *src, void *dst);

	BEFORE_ORIGINAL_SOCK(inet_pton, LIBC);

	ret = inet_ptonp(af, src, dst);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dpp", af, voidp_to_uint64(src), voidp_to_uint64(dst));

	return ret;
}

int getaddrinfo(const char *node, const char *service,
		const struct addrinfo *hints, struct addrinfo **res) {
	static int (*getaddrinfop)(const char *node, const char *service,
			const struct addrinfo *hints, struct addrinfo **res);

	BEFORE_ORIGINAL_NOFILTER(getaddrinfo, LIBC);

	ret = getaddrinfop(node, service, hints, res);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pppp", voidp_to_uint64(node), voidp_to_uint64(service), voidp_to_uint64(hints), voidp_to_uint64(res));

	return ret;
}

void freeaddrinfo(struct addrinfo *res) {
	static void (*freeaddrinfop)(struct addrinfo *res);

	BEFORE_ORIGINAL_NOFILTER(freeaddrinfo, LIBC);

	freeaddrinfop(res);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(res));
}

const char *gai_strerror(int errcode) {
	static const char * (*gai_strerrorp)(int errcode);
	const char * cret;

	BEFORE_ORIGINAL_SOCK(gai_strerror, LIBC);

	cret = gai_strerrorp(errcode);

	AFTER_ORIGINAL_LIBC_SOCK(cret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", errcode);

	return cret;
}

int gai_suspend(const struct gaicb* const list[], int nitems,
		const struct timespec *timeout) {
	static int (*gai_suspendp)(const struct gaicb* const list[], int nitems,
			const struct timespec *timeout);

	BEFORE_ORIGINAL_SOCK(gai_suspend, LIBC);

	ret = gai_suspendp(list, nitems, timeout);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pdp", voidp_to_uint64(list), nitems, voidp_to_uint64(timeout));

	return ret;
}

int gai_error(struct gaicb *req) {
	static int (*gai_errorp)(struct gaicb *req);

	BEFORE_ORIGINAL_SOCK(gai_error, LIBC);

	ret = gai_errorp(req);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(req));

	return ret;
}

int gai_cancel(struct gaicb *req) {
	static int (*gai_cancelp)(struct gaicb *req);

	BEFORE_ORIGINAL_SOCK(gai_cancel, LIBC);

	ret = gai_cancelp(req);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(req));

	return ret;
}

int getaddrinfo_a(int mode, struct gaicb *list[], int nitems,
		struct sigevent *sevp) {
	static int (*getaddrinfo_ap)(int mode, struct gaicb *list[], int nitems,
			struct sigevent *sevp);

	BEFORE_ORIGINAL_SOCK(getaddrinfo_a, LIBC);

	ret = getaddrinfo_ap(mode, list, nitems, sevp);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dpdp", mode, voidp_to_uint64(list), nitems, voidp_to_uint64(sevp));

	return ret;
}

int getdomainname(char *name, size_t len) {
	static int (*getdomainnamep)(char *name, size_t len);

	BEFORE_ORIGINAL_SOCK(getdomainname, LIBC);

	ret = getdomainnamep(name, len);

	//AFTER_ORIGINAL_NOSOCK(FD_API_OTHER, "pd", voidp_to_uint64(name), len);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pd", voidp_to_uint64(name), len);

	return ret;
}

int setdomainname(const char *name, size_t len) {
	static int (*setdomainnamep)(const char *name, size_t len);

	BEFORE_ORIGINAL_SOCK(setdomainname, LIBC);

	ret = setdomainnamep(name, len);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pd", voidp_to_uint64(name), len);

	return ret;
}

int gethostname(char *name, size_t len) {
	static int (*gethostnamep)(char *name, size_t len);

	BEFORE_ORIGINAL_SOCK(gethostname, LIBC);

	ret = gethostnamep(name, len);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pd", voidp_to_uint64(name), len);

	return ret;
}

int sethostname(const char *name, size_t len) {
	static int (*sethostnamep)(const char *name, size_t len);

	BEFORE_ORIGINAL_SOCK(sethostname, LIBC);

	ret = sethostnamep(name, len);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pd", voidp_to_uint64(name), len);

	return ret;
}

int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
		socklen_t hostlen, char *serv, socklen_t servlen, unsigned int flags) {
	static int (*getnameinfop)(const struct sockaddr *sa, socklen_t salen,
			char *host, socklen_t hostlen, char *serv, socklen_t servlen,
			unsigned int flags);

	BEFORE_ORIGINAL_SOCK(getnameinfo, LIBC);

	ret = getnameinfop(sa, salen, host, hostlen, serv, servlen, flags);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pdpdpdd", voidp_to_uint64(sa), salen,
				 voidp_to_uint64(host), hostlen,
				 voidp_to_uint64(serv), servlen, flags);

	return ret;
}

struct hostent *gethostbyname(const char *name) {
	static struct hostent * (*gethostbynamep)(const char *name);
	struct hostent* pret;

	BEFORE_ORIGINAL_SOCK(gethostbyname, LIBC);

	pret = gethostbynamep(name);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(name));

	return pret;
}

struct hostent *gethostbyaddr(const void *addr, socklen_t len, int type) {
	static struct hostent * (*gethostbyaddrp)(const void *addr, socklen_t len,
			int type);
	struct hostent* pret;

	BEFORE_ORIGINAL_SOCK(gethostbyaddr, LIBC);

	pret = gethostbyaddrp(addr, len, type);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pdd", voidp_to_uint64(addr), len, type);

	return pret;
}

void sethostent(int stayopen) {
	static void (*sethostentp)(int stayopen);

	BEFORE_ORIGINAL_SOCK(sethostent, LIBC);

	sethostentp(stayopen);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", stayopen);
}

void endhostent(void) {
	static void (*endhostentp)(void);

	BEFORE_ORIGINAL_SOCK(endhostent, LIBC);

	endhostentp();

	//AFTER_ORIGINAL_NOSOCK_RET(NULL, 0, FD_API_OTHER, "s", "");

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");
}

void herror(const char *s) {
	static void (*herrorp)(const char *s);

	BEFORE_ORIGINAL_SOCK(herror, LIBC);

	herrorp(s);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(s));
}

const char *hstrerror(int err) {
	static const char* (*hstrerrorp)(int err);
	const char* cret;

	BEFORE_ORIGINAL_SOCK(hstrerror, LIBC);

	cret = hstrerrorp(err);

	AFTER_ORIGINAL_LIBC_SOCK(cret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", err);

	return cret;
}

struct hostent *gethostent(void) {
	static struct hostent* (*gethostentp)(void);
	struct hostent* pret;

	BEFORE_ORIGINAL_SOCK(gethostent, LIBC);

	pret = gethostentp();

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");

	return pret;
}

struct hostent *gethostbyname2(const char *name, int af) {
	static struct hostent * (*gethostbyname2p)(const char *name, int af);
	struct hostent* pret;

	BEFORE_ORIGINAL_SOCK(gethostbyname2, LIBC);

	pret = gethostbyname2p(name, af);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pd", voidp_to_uint64(name), af);

	return pret;
}

int gethostent_r(struct hostent *rret, char *buf, size_t buflen,
		struct hostent **result, int *h_errnop) {
	static int (*gethostent_rp)(struct hostent *rret, char *buf, size_t buflen,
			struct hostent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(gethostent_r, LIBC);

	ret = gethostent_rp(rret, buf, buflen, result, h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "ppdpp", voidp_to_uint64(rret),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

int gethostbyaddr_r(const void *addr, socklen_t len, int type,
		struct hostent *rret, char *buf, size_t buflen, struct hostent **result,
		int *h_errnop) {
	static int (*gethostbyaddr_rp)(const void *addr, socklen_t len, int type,
			struct hostent *rret, char *buf, size_t buflen,
			struct hostent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(gethostbyaddr_r, LIBC);

	ret = gethostbyaddr_rp(addr, len, type, rret, buf, buflen, result,
			h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pddppdpp", voidp_to_uint64(addr), len,
				 type, voidp_to_uint64(rret),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

int gethostbyname_r(const char *name, struct hostent *rret, char *buf,
		size_t buflen, struct hostent **result, int *h_errnop) {
	static int (*gethostbyname_rp)(const char *name, struct hostent *rret,
			char *buf, size_t buflen, struct hostent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(gethostbyname_r, LIBC);

	ret = gethostbyname_rp(name, rret, buf, buflen, result, h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pppdpp", voidp_to_uint64(name),
				 voidp_to_uint64(rret), voidp_to_uint64(buf),
				 buflen, voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

int gethostbyname2_r(const char *name, int af, struct hostent *rret, char *buf,
		size_t buflen, struct hostent **result, int *h_errnop) {
	static int (*gethostbyname2_rp)(const char *name, int af,
			struct hostent *rret, char *buf, size_t buflen,
			struct hostent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(gethostbyname2_r, LIBC);

	ret = gethostbyname2_rp(name, af, rret, buf, buflen, result, h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pdppdpp", voidp_to_uint64(name), af,
				 voidp_to_uint64(rret), voidp_to_uint64(buf),
				 buflen, voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

struct servent *getservbyname(const char *name, const char *proto) {
	static struct servent * (*getservbynamep)(const char *name,
			const char *proto);
	struct servent* pret;

	BEFORE_ORIGINAL_SOCK(getservbyname, LIBC);

	pret = getservbynamep(name, proto);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pp", voidp_to_uint64(name),
				 voidp_to_uint64(proto));

	return pret;
}

void setservent(int stayopen) {
	static void (*setserventp)(int stayopen);

	BEFORE_ORIGINAL_SOCK(setservent, LIBC);

	setserventp(stayopen);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", stayopen);
}

void endservent(void) {
	static void (*endserventp)(void);

	BEFORE_ORIGINAL_SOCK(endservent, LIBC);

	endserventp();

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");
}

struct servent *getservent(void) {
	static struct servent * (*getserventp)(void);
	struct servent* pret;

	BEFORE_ORIGINAL_SOCK(getservent, LIBC);

	pret = getserventp();

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");

	return pret;
}

struct servent *getservbyport(int port, const char *proto) {
	static struct servent * (*getservbyportp)(int port, const char *proto);
	struct servent* pret;

	BEFORE_ORIGINAL_SOCK(getservbyport, LIBC);

	pret = getservbyportp(port, proto);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dp", port, voidp_to_uint64(proto));

	return pret;
}

int getservent_r(struct servent *result_buf, char *buf, size_t buflen,
		struct servent **result) {
	static int (*getservent_rp)(struct servent *result_buf, char *buf,
			size_t buflen, struct servent **result);

	BEFORE_ORIGINAL_SOCK(getservent_r, LIBC);

	ret = getservent_rp(result_buf, buf, buflen, result);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "ppdp", voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result));

	return ret;
}

int getservbyname_r(const char *name, const char *proto,
		struct servent *result_buf, char *buf, size_t buflen,
		struct servent **result) {
	static int (*getservbyname_rp)(const char *name, const char *proto,
			struct servent *result_buf, char *buf, size_t buflen,
			struct servent **result);

	BEFORE_ORIGINAL_SOCK(getservbyname_r, LIBC);

	ret = getservbyname_rp(name, proto, result_buf, buf, buflen, result);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "ppppdp", voidp_to_uint64(name),
				 voidp_to_uint64(proto),
				 voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result));

	return ret;
}

int getservbyport_r(int port, const char *proto, struct servent *result_buf,
		char *buf, size_t buflen, struct servent **result) {
	static int (*getservbyport_rp)(int port, const char *proto,
			struct servent *result_buf, char *buf, size_t buflen,
			struct servent **result);

	BEFORE_ORIGINAL_SOCK(getservbyport_r, LIBC);

	ret = getservbyport_rp(port, proto, result_buf, buf, buflen, result);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dpppdp", port, voidp_to_uint64(proto),
				 voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result));

	return ret;
}

struct netent* getnetent(void) {
	static struct netent * (*getnetentp)(void);
	struct netent* pret;

	BEFORE_ORIGINAL_SOCK(getnetent, LIBC);

	pret = getnetentp();

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");

	return pret;
}

struct netent *getnetbyname(const char *name) {
	static struct netent * (*getnetbynamep)(const char *name);
	struct netent* pret;

	BEFORE_ORIGINAL_SOCK(getnetbyname, LIBC);

	pret = getnetbynamep(name);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(name));

	return pret;
}

struct netent *getnetbyaddr(uint32_t net, int type) {
	static struct netent * (*getnetbyaddrp)(uint32_t net, int type);
	struct netent * pret;

	BEFORE_ORIGINAL_SOCK(getnetbyaddr, LIBC);

	pret = getnetbyaddrp(net, type);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "dd", net, type);

	return pret;
}

void setnetent(int stayopen) {
	static void (*setnetentp)(int stayopen);

	BEFORE_ORIGINAL_SOCK(setnetent, LIBC);

	setnetentp(stayopen);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", stayopen);
}

void endnetent(void) {
	static void (*endnetentp)(void);

	BEFORE_ORIGINAL_SOCK(endnetent, LIBC);

	endnetentp();

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");
}

int getnetent_r(struct netent *result_buf, char *buf, size_t buflen,
		struct netent **result, int *h_errnop) {
	static int (*getnetent_rp)(struct netent *result_buf, char *buf,
			size_t buflen, struct netent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(getnetent_r, LIBC);

	ret = getnetent_rp(result_buf, buf, buflen, result, h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "ppdpp", voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

int getnetbyname_r(const char *name, struct netent *result_buf, char *buf,
		size_t buflen, struct netent **result, int *h_errnop) {
	static int (*getnetbyname_rp)(const char *name, struct netent *result_buf,
			char *buf, size_t buflen, struct netent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(getnetbyname_r, LIBC);

	ret = getnetbyname_rp(name, result_buf, buf, buflen, result, h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pppdpp", voidp_to_uint64(name),
				 voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

int getnetbyaddr_r(uint32_t net, int type, struct netent *result_buf, char *buf,
		size_t buflen, struct netent **result, int *h_errnop) {
	static int (*getnetbyaddr_rp)(uint32_t net, int type,
			struct netent *result_buf, char *buf, size_t buflen,
			struct netent **result, int *h_errnop);

	BEFORE_ORIGINAL_SOCK(getnetbyaddr_r, LIBC);

	ret = getnetbyaddr_rp(net, type, result_buf, buf, buflen, result, h_errnop);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "ddppdpp", net, type,
				 voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result),
				 voidp_to_uint64(h_errnop));

	return ret;
}

struct protoent *getprotoent(void) {
	static struct protoent * (*getprotoentp)(void);
	struct protoent * pret;

	BEFORE_ORIGINAL_SOCK(getprotoent, LIBC);

	pret = getprotoentp();

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");

	return pret;
}

struct protoent *getprotobyname(const char *name) {
	static struct protoent * (*getprotobynamep)(const char *name);
	struct protoent * pret;

	BEFORE_ORIGINAL_SOCK(getprotobyname, LIBC);

	pret = getprotobynamep(name);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(name));

	return pret;
}

struct protoent *getprotobynumber(int proto) {
	static struct protoent * (*getprotobynumberp)(int proto);
	struct protoent * pret;

	BEFORE_ORIGINAL_SOCK(getprotobynumber, LIBC);

	pret = getprotobynumberp(proto);

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", proto);

	return pret;
}

void setprotoent(int stayopen) {
	static void (*setprotoentp)(int stayopen);

	BEFORE_ORIGINAL_SOCK(setprotoent, LIBC);

	setprotoentp(stayopen);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "d", stayopen);
}

void endprotoent(void) {
	static void (*endprotoentp)(void);

	BEFORE_ORIGINAL_SOCK(endprotoent, LIBC);

	endprotoentp();

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");
}

int getprotoent_r(struct protoent *result_buf, char *buf, size_t buflen,
		struct protoent **result) {
	static int (*getprotoent_rp)(struct protoent *result_buf, char *buf,
			size_t buflen, struct protoent **result);

	BEFORE_ORIGINAL_SOCK(getprotoent_r, LIBC);

	ret = getprotoent_rp(result_buf, buf, buflen, result);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "ppdp", voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result));

	return ret;
}

int getprotobyname_r(const char *name, struct protoent *result_buf, char *buf,
		size_t buflen, struct protoent **result) {
	static int (*getprotobyname_rp)(const char *name,
			struct protoent *result_buf, char *buf, size_t buflen,
			struct protoent **result);

	BEFORE_ORIGINAL_SOCK(getprotobyname_r, LIBC);

	ret = getprotobyname_rp(name, result_buf, buf, buflen, result);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "pppdp", voidp_to_uint64(name),
				 voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result));

	return ret;
}

int getprotobynumber_r(int proto, struct protoent *result_buf, char *buf,
		size_t buflen, struct protoent **result) {
	static int (*getprotobynumber_rp)(int proto, struct protoent *result_buf,
			char *buf, size_t buflen, struct protoent **result);

	BEFORE_ORIGINAL_SOCK(getprotobynumber_r, LIBC);

	ret = getprotobynumber_rp(proto, result_buf, buf, buflen, result);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dppdp", proto,
				 voidp_to_uint64(result_buf),
				 voidp_to_uint64(buf), buflen,
				 voidp_to_uint64(result));

	return ret;
}

unsigned int if_nametoindex(__const char *__ifname) {
	static unsigned int (*if_nametoindexp)(__const char *__ifname);
	unsigned int uret;

	BEFORE_ORIGINAL_SOCK(if_nametoindex, LIBC);

	uret = if_nametoindexp(__ifname);

	AFTER_ORIGINAL_LIBC_SOCK(uret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(__ifname));

	return uret;
}

char *if_indextoname(unsigned int __ifindex, char *__ifname) {
	static char * (*if_indextonamep)(unsigned int __ifindex, char *__ifname);
	char * cret;

	BEFORE_ORIGINAL_SOCK(if_indextoname, LIBC);

	cret = if_indextonamep(__ifindex, __ifname);

	AFTER_ORIGINAL_LIBC_SOCK(cret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "dp", __ifindex,
				 voidp_to_uint64(__ifname));

	return cret;
}

struct if_nameindex *if_nameindex(void) {
	static struct if_nameindex * (*if_nameindexp)(void);
	struct if_nameindex * pret;

	BEFORE_ORIGINAL_NOFILTER(if_nameindex, LIBC);

	pret = if_nameindexp();

	AFTER_ORIGINAL_LIBC_SOCK(pret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
			info, "s", "");

	return pret;
}

void if_freenameindex(struct if_nameindex *__ptr) {
	static void (*if_freenameindexp)(struct if_nameindex *__ptr);

	BEFORE_ORIGINAL_NOFILTER(if_freenameindex, LIBC);

	if_freenameindexp(__ptr);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(__ptr));
}

int getifaddrs(struct ifaddrs **ifap) {
	static int (*getifaddrsp)(struct ifaddrs **ifap);

	BEFORE_ORIGINAL_NOFILTER(getifaddrs, LIBC);

	ret = getifaddrsp(ifap);

	AFTER_ORIGINAL_LIBC_SOCK('d', ret, OBJ_DUMMY, 0, SOCKET_API_OTHER,
				 info, "p", voidp_to_uint64(ifap));

	return ret;
}

void freeifaddrs(struct ifaddrs *ifa) {
	static void (*freeifaddrsp)(struct ifaddrs *ifa);

	BEFORE_ORIGINAL_NOFILTER(freeifaddrs, LIBC);

	freeifaddrsp(ifa);

	AFTER_ORIGINAL_LIBC_SOCK(NULL, OBJ_DUMMY, 0, SOCKET_API_OTHER, info,
				 "p", voidp_to_uint64(ifa));
}

//To do

uint16_t htobe16(uint16_t host_16bits)
{
	static uint16_t (*htobe16p)(uint16_t host_16bits);
	uint16_t uret;

	BEFORE_ORIGINAL_SOCK(htobe16, LIBC);

	uret = htobe16p(host_16bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', (uint32_t)uret, 0, FD_API_OTHER, "d", host_16bits);

	return uret;
}

uint16_t htole16(uint16_t host_16bits)
{
	static uint16_t (*htole16p)(uint16_t host_16bits);
	uint16_t uret;

	BEFORE_ORIGINAL_SOCK(htole16, LIBC);

	uret = htole16p(host_16bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', (uint32_t)uret, 0, FD_API_OTHER, "d", host_16bits);

	return uret;
}

uint16_t be16toh(uint16_t big_endian_16bits)
{
	static uint16_t (*be16tohp)(uint16_t big_endian_16bits);
	uint16_t uret;

	BEFORE_ORIGINAL_SOCK(be16toh, LIBC);

	uret = be16tohp(big_endian_16bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', (uint32_t)uret, 0, FD_API_OTHER,
			"d", big_endian_16bits);

	return uret;
}

uint16_t le16toh(uint16_t little_endian_16bits)
{
	static uint16_t (*le16tohp)(uint16_t little_endian_16bits);
	uint16_t uret;

	BEFORE_ORIGINAL_SOCK(le16toh, LIBC);

	uret = le16tohp(little_endian_16bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', (uint32_t)uret, 0, FD_API_OTHER,
			"d", little_endian_16bits);

	return uret;
}

uint32_t htobe32(uint32_t host_32bits)
{
	static uint32_t (*htobe32p)(uint32_t host_32bits);
	uint32_t uret;

	BEFORE_ORIGINAL_SOCK(htobe32, LIBC);

	uret = htobe32p(host_32bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', uret, 0, FD_API_OTHER,
			"d", host_32bits);

	return uret;
}

uint32_t htole32(uint32_t host_32bits)
{
	static uint32_t (*htole32p)(uint32_t host_32bits);
	uint32_t uret;

	BEFORE_ORIGINAL_SOCK(htole32, LIBC);

	uret = htole32p(host_32bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', uret, 0, FD_API_OTHER,
			"d", host_32bits);

	return uret;
}

uint32_t be32toh(uint32_t big_endian_32bits)
{
	static uint32_t (*be32tohp)(uint32_t big_endian_32bits);
	uint32_t uret;

	BEFORE_ORIGINAL_SOCK(be32toh, LIBC);

	uret = be32tohp(big_endian_32bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', uret, 0, FD_API_OTHER,
			"d", big_endian_32bits);

	return uret;
}

uint32_t le32toh(uint32_t little_endian_32bits)
{
	static uint32_t (*le32tohp)(uint32_t little_endian_32bits);
	uint32_t uret;

	BEFORE_ORIGINAL_SOCK(le32toh, LIBC);

	uret = le32tohp(little_endian_32bits);

	AFTER_ORIGINAL_NOSOCK_RET('d', uret, 0, FD_API_OTHER,
			"d", little_endian_32bits);

	return uret;
}

uint64_t htobe64(uint64_t host_64bits)
{
	static uint64_t (*htobe64p)(uint64_t host_64bits);
	uint64_t uret;

	BEFORE_ORIGINAL_SOCK(htobe64, LIBC);

	uret = htobe64p(host_64bits);

	AFTER_ORIGINAL_NOSOCK_RET('x', uret, 0, FD_API_OTHER,
			"d", host_64bits);

	return uret;
}

uint64_t htole64(uint64_t host_64bits)
{
	static uint64_t (*htole64p)(uint64_t host_64bits);
	uint64_t uret;

	BEFORE_ORIGINAL_SOCK(htole64, LIBC);

	uret = htole64p(host_64bits);

	AFTER_ORIGINAL_NOSOCK_RET('x', uret, 0, FD_API_OTHER,
			"d", host_64bits);

	return uret;
}

uint64_t be64toh(uint64_t big_endian_64bits)
{
	static uint64_t (*be64tohp)(uint64_t big_endian_64bits);
	uint64_t uret;

	BEFORE_ORIGINAL_SOCK(be64toh, LIBC);

	uret = be64tohp(big_endian_64bits);

	AFTER_ORIGINAL_NOSOCK_RET('x', uret, 0, FD_API_OTHER,
			"d", big_endian_64bits);

	return uret;
}

uint64_t le64toh(uint64_t little_endian_64bits)
{
	static uint64_t (*le64tohp)(uint64_t little_endian_64bits);
	uint64_t uret;

	BEFORE_ORIGINAL_SOCK(le64toh, LIBC);

	uret = le64tohp(little_endian_64bits);

	AFTER_ORIGINAL_NOSOCK_RET('x', uret, 0, FD_API_OTHER,
			"d", little_endian_64bits);

	return uret;
}

struct in_addr inet_makeaddr(int net, int host)
{
	static struct in_addr (*inet_makeaddrp)(int net, int host);
	struct in_addr iret;

	BEFORE_ORIGINAL_SOCK(inet_makeaddr, LIBC);

	iret = inet_makeaddrp(net,host);

	AFTER_ORIGINAL_NOSOCK_RET('d', iret.s_addr, 0, FD_API_OTHER,
			"dd", net, host);

	return iret;
}

#endif
