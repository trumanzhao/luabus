/*
** repository: https://github.com/trumanzhao/luabus
** trumanzhao, 2020-01-05, trumanzhao@foxmail.com
*/

#ifdef _MSC_VER
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <mstcpip.h>
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <algorithm>
#include <assert.h>
#include "tools.h"
#include "var_int.h"
#include "socket_mgr.h"
#include "socket_node.h"

void socket_node::close() {
    if (m_socket != INVALID_SOCKET) {
        m_mgr->unwatch(m_socket);
        close_socket_handle(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_closed = true; 
}
