/*
** repository: https://github.com/trumanzhao/luabus
** trumanzhao, 2020-01-05, trumanzhao@foxmail.com
*/

#pragma once

#include <functional>
#include "tools.h"
#include "socket_helper.h"

class socket_mgr;

struct socket_node {
    socket_node(uint32_t token) : m_token(token) {}
    virtual ~socket_node() {};
    void close();
    virtual bool get_remote_ip(std::string& ip) = 0;
    virtual void connect(const char node_name[], const char service_name[]) { }
    virtual void set_send_buffer_size(size_t size) { }
    virtual void set_recv_buffer_size(size_t size) { }
    virtual void set_nodelay(int flag) { }
    virtual void send(const void* data, size_t data_len) { }
    virtual void set_accept_callback(const std::function<void(int)>& cb) { }
    virtual void set_connect_callback(const std::function<void(bool, const char*)>& cb) { }
    virtual void set_package_callback(const std::function<void(char*, size_t)>& cb) { }
    virtual void set_error_callback(const std::function<void(const char*)>& cb) { }

#ifdef _MSC_VER
    virtual void on_complete(WSAOVERLAPPED* ovl) = 0;
#endif

#if defined(__linux) || defined(__APPLE__)
    virtual void on_can_recv(size_t data_len = UINT_MAX, bool is_eof = false) {};
    virtual void on_can_send(size_t data_len = UINT_MAX, bool is_eof = false) {};
#endif

    uint32_t m_token;
    socket_mgr* m_mgr = nullptr;
    socket_t m_socket = INVALID_SOCKET;
    bool m_connected = false;    
    bool m_io_handing = false;
    bool m_closed = false;
    int m_ovl_ref = 0;
};
