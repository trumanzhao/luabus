/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

#pragma once

#include <limits.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <set>
#include <list>
#include "socket_helper.h"
#include "socket_node.h"

class socket_mgr {
public:
    socket_mgr();
    ~socket_mgr();

    bool setup(int max_connection);

#ifdef _MSC_VER
    bool get_socket_funcs();
#endif

    int wait(int timeout);

    uint32_t listen(std::string& err, const char ip[], int port, int backlog);
    uint32_t connect(std::string& err, const char node_name[], const char service_name[], int timeout);

    void set_send_buffer_size(uint32_t token, size_t size);
    void set_recv_buffer_size(uint32_t token, size_t size);
    void set_nodelay(uint32_t token, int flag);
    void send(uint32_t token, const void* data, size_t data_len);
    void close(uint32_t token);
    bool get_remote_ip(uint32_t token, std::string& ip);

    void set_accept_callback(uint32_t token, const std::function<void(uint32_t)>& cb);
    void set_connect_callback(uint32_t token, const std::function<void(bool, const char*)>& cb);
    void set_package_callback(uint32_t token, const std::function<void(char*, size_t)>& cb);
    void set_error_callback(uint32_t token, const std::function<void(const char*)>& cb);

    bool watch_listen(socket_t fd, socket_node* node);
    bool watch_accepted(socket_t fd, socket_node* node);
    bool watch_connecting(socket_t fd, socket_node* node);
    bool watch_connected(socket_t fd, socket_node* node);
    bool watch_send(socket_t fd, socket_node* node, bool enable);
    void unwatch(socket_t fd);
    int accept_stream(socket_t fd, const char ip[]);

    void increase_count() { m_count++; }
    void decrease_count() { m_count--; }
    bool is_full() { return m_count >= m_max_count; }
    socket_node* find_node(int token);
    uint32_t new_token();
private:

#ifdef _MSC_VER
    LPFN_ACCEPTEX m_accept_func = nullptr;
    LPFN_CONNECTEX m_connect_func = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS m_addrs_func = nullptr;
    HANDLE m_handle = INVALID_HANDLE_VALUE;
    std::vector<OVERLAPPED_ENTRY> m_events;
#endif

#ifdef __linux
    int m_handle = -1;
    std::vector<epoll_event> m_events;
#endif

#ifdef __APPLE__
    int m_handle = -1;
    std::vector<struct kevent> m_events;
    std::vector<socket_node*> m_close_list;
#endif

    struct timeout_node_t {
        uint32_t m_token;
        int64_t m_deadline;
        bool operator < (const timeout_node_t& other) const {
            if (m_deadline == other.m_deadline) {
                return m_token < other.m_token;
            }
            return m_deadline < other.m_deadline;
        }
    };
    std::set<timeout_node_t> m_timeout_list;
    std::list<std::function<void()>> m_delay_calls;

    int m_max_count = 0;
    int m_count = 0;
    uint32_t m_next_token = 0;
    int64_t m_next_update = 0;
    std::unordered_map<uint32_t, socket_node*> m_nodes;
};
