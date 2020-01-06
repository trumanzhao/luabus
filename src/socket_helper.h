/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#endif
#ifdef __linux
#include <sys/epoll.h>
#endif
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#if defined(__linux) || defined(__APPLE__)
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#if defined(__linux) || defined(__APPLE__)
#include <errno.h>
using socket_t = int;
const socket_t INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
inline int get_socket_error() { return errno; }
inline void close_socket_handle(socket_t fd) { close(fd); }
#endif

#ifdef _MSC_VER
using socket_t = SOCKET;
inline int get_socket_error() { return WSAGetLastError(); }
inline void close_socket_handle(socket_t fd) { closesocket(fd); }
bool wsa_send_empty(socket_t fd, WSAOVERLAPPED& ovl);
bool wsa_recv_empty(socket_t fd, WSAOVERLAPPED& ovl);
#endif

bool make_ip_addr(sockaddr_storage* addr, size_t* len, const char ip[], int port);
// ip字符串建议大小: char ip[INET6_ADDRSTRLEN];
bool get_ip_string(char ip[], size_t ip_size, const void* addr, size_t addr_len);

void set_no_block(socket_t fd);
void set_no_delay(socket_t fd, int enable);
void set_close_on_exec(socket_t fd);


