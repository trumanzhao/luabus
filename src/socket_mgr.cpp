/*
** repository: https://github.com/trumanzhao/luna
** trumanzhao, 2016-11-01, trumanzhao@foxmail.com
*/

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "tools.h"
#include "socket_helper.h"
#include "io_buffer.h"
#include "socket_mgr.h"
#include "socket_stream.h"
#include "socket_listener.h"

#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif

socket_mgr::socket_mgr() {
#ifdef _MSC_VER
    WORD    wVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    WSAStartup(wVersion, &wsaData);
#endif
}

socket_mgr::~socket_mgr() {
    for (auto& node : m_nodes) {
        delete node.second;
    }

#ifdef _MSC_VER
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
    WSACleanup();
#endif

#ifdef __linux
    if (m_handle != -1) {
        ::close(m_handle);
        m_handle = -1;
    }
#endif

#ifdef __APPLE__
    if (m_handle != -1) {
        ::close(m_handle);
        m_handle = -1;
    }
#endif
}

bool socket_mgr::setup(int max_connection) {
#ifdef _MSC_VER
    m_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_handle == INVALID_HANDLE_VALUE)
        return false;

    if (!get_socket_funcs())
        return false;
#endif

#ifdef __linux
    m_handle = epoll_create(max_connection);
    if (m_handle == -1)
        return false;
#endif

#ifdef __APPLE__
    m_handle = kqueue();
    if (m_handle == -1)
        return false;
#endif

    m_max_count = max_connection;
    m_events.resize(max_connection);

    return true;
}

#ifdef _MSC_VER
bool socket_mgr::get_socket_funcs() {
    bool result = false;
    int ret = 0;
    socket_t fd = INVALID_SOCKET;
    DWORD bytes = 0;
    GUID func_guid = WSAID_ACCEPTEX;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    FAILED_JUMP(fd != INVALID_SOCKET);

    bytes = 0;
    func_guid = WSAID_ACCEPTEX;
    ret = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid), &m_accept_func, sizeof(m_accept_func), &bytes, nullptr, nullptr);
    FAILED_JUMP(ret != SOCKET_ERROR);

    bytes = 0;
    func_guid = WSAID_CONNECTEX;
    ret = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid), &m_connect_func, sizeof(m_connect_func), &bytes, nullptr, nullptr);
    FAILED_JUMP(ret != SOCKET_ERROR);

    bytes = 0;
    func_guid = WSAID_GETACCEPTEXSOCKADDRS;
    ret = WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER, &func_guid, sizeof(func_guid), &m_addrs_func, sizeof(m_addrs_func), &bytes, nullptr, nullptr);
    FAILED_JUMP(ret != SOCKET_ERROR);

    result = true;
Exit0:
    if (fd != INVALID_SOCKET) {
        close_socket_handle(fd);
        fd = INVALID_SOCKET;
    }
    return result;
}
#endif

int socket_mgr::wait(int timeout) {
#ifdef _MSC_VER
    ULONG event_count = 0;
    int ret = GetQueuedCompletionStatusEx(m_handle, &m_events[0], (ULONG)m_events.size(), &event_count, (DWORD)timeout, false);
    if (ret) {
        for (ULONG i = 0; i < event_count; i++) {
            OVERLAPPED_ENTRY& oe = m_events[i];
            auto node = (socket_node*)oe.lpCompletionKey;

            node->m_ovl_ref--;
            if (!node->m_closed) {
                node->m_io_handing = true;
                node->on_complete(oe.lpOverlapped);
                node->m_io_handing = false;
            }
                    
            if (node->m_closed && node->m_ovl_ref == 0) {
                m_nodes.erase(node->m_token);
                delete node;
            }
        }
    }
#endif

#ifdef __linux
    int event_count = epoll_wait(m_handle, &m_events[0], (int)m_events.size(), timeout);
    for (int i = 0; i < event_count; i++) {
        epoll_event& ev = m_events[i];
        auto node = (socket_node*)ev.data.ptr;

        node->m_io_handing = true;
        if (ev.events & EPOLLIN != 0 && !node->m_closed) {
            node->on_can_recv();
        }

        if (ev.events & EPOLLOUT != 0 && !node->m_closed) {
            node->on_can_send();
        }        
        node->m_io_handing = false;

        if (node->m_closed) {
            m_nodes.erase(node->m_token);
            delete node;
        }
    }
#endif

#ifdef __APPLE__
    timespec time_wait;
    time_wait.tv_sec = timeout / 1000;
    time_wait.tv_nsec = (timeout % 1000) * 1000000;
    int event_count = kevent(m_handle, nullptr, 0, &m_events[0], (int)m_events.size(), timeout >= 0 ? &time_wait : nullptr);
    for (int i = 0; i < event_count; i++) {
        struct kevent& ev = m_events[i];
        auto node = (socket_node*)ev.udata;
        if (!node->m_closed) {
            node->m_io_handing = true;
            if (ev.filter == EVFILT_READ) {
                node->on_can_recv((size_t)ev.data, (ev.flags & EV_EOF) != 0);
            } else if (ev.filter == EVFILT_WRITE) {
                node->on_can_send((size_t)ev.data, (ev.flags & EV_EOF) != 0);
            }
            node->m_io_handing = false;
            if (node->m_closed) {
                m_close_list.push_back(node);
            }
        }
    }

    for (auto node : m_close_list) {
        m_nodes.erase(node->m_token);
        delete node;
    }
    m_close_list.clear();
#endif
	return (int)event_count;
}

int socket_mgr::listen(std::string& err, const char ip[], int port) {
    int ret = false;
    socket_t fd = INVALID_SOCKET;
    sockaddr_storage addr;
    size_t addr_len = 0;
    int one = 1;
    int token = new_token();    

#ifdef _MSC_VER
    auto* listener = new socket_listener(token, this, m_accept_func, m_addrs_func);
#endif

#if defined(__linux) || defined(__APPLE__)
    auto* listener = new socket_listener(token, this);
#endif

    ret = make_ip_addr(&addr, &addr_len, ip, port);
    FAILED_JUMP(ret);

    fd = socket(addr.ss_family, SOCK_STREAM, IPPROTO_IP);
    FAILED_JUMP(fd != INVALID_SOCKET);

    set_no_block(fd);
    set_close_on_exec(fd);

    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));
    FAILED_JUMP(ret != SOCKET_ERROR);

    // macOSX require addr_len to be the real len (ipv4/ipv6)
    ret = bind(fd, (sockaddr*)&addr, (int)addr_len);
    FAILED_JUMP(ret != SOCKET_ERROR);

    ret = ::listen(fd, 16);
    FAILED_JUMP(ret != SOCKET_ERROR);

    if (watch_listen(fd, listener) && listener->setup(fd)) {
        m_nodes[token] = listener;
        return token;
    }

Exit0:
    get_error_string(err, get_socket_error());
    delete listener;
    if (fd != INVALID_SOCKET) {
        close_socket_handle(fd);
        fd = INVALID_SOCKET;
    }
    return 0;
}

int socket_mgr::connect(std::string& err, const char node_name[], const char service_name[], int timeout) {
    if (is_full()) {
        err = "too-many-connection";
        return 0;
    }

    int token = new_token();    

#ifdef _MSC_VER
    socket_stream* stm = new socket_stream(token, this, m_connect_func);
#endif

#if defined(__linux) || defined(__APPLE__)
    socket_stream* stm = new socket_stream(token, this);
#endif

    stm->connect(node_name, service_name, timeout);
    m_nodes[token] = stm;
    return token;
}

void socket_mgr::set_send_buffer_size(uint32_t token, size_t size) {
    auto node = find_node(token);
    if (node && size > 0) {
        node->set_send_buffer_size(size);
    }
}

void socket_mgr::set_recv_buffer_size(uint32_t token, size_t size) {
    auto node = find_node(token);
    if (node && size > 0) {
        node->set_recv_buffer_size(size);
    }
}

void socket_mgr::set_nodelay(uint32_t token, int flag) {
    auto node = find_node(token);
    if (node) {
        node->set_nodelay(flag);
    }
}

void socket_mgr::send(uint32_t token, const void* data, size_t data_len) {
    auto node = find_node(token);
    if (node) {
        node->send(data, data_len);
    }
}

void socket_mgr::close(uint32_t token) {
    auto node = find_node(token);
    if (node && !node->m_closed) {
        node->close();
        if (node->m_ovl_ref == 0 && !node->m_io_handing) {
            m_nodes.erase(node->m_token);
            delete node;            
        }
    }
}

bool socket_mgr::get_remote_ip(uint32_t token, std::string& ip) {
    auto node = find_node(token);
    if (node) {
        return node->get_remote_ip(ip);
    }
    return false;
}

void socket_mgr::set_accept_callback(uint32_t token, const std::function<void(uint32_t)>& cb) {
    auto node = find_node(token);
    if (node) {
        node->set_accept_callback(cb);
    }
}

void socket_mgr::set_connect_callback(uint32_t token, const std::function<void(bool, const char*)>& cb) {
    auto node = find_node(token);
    if (node) {
        node->set_connect_callback(cb);
    }
}

void socket_mgr::set_package_callback(uint32_t token, const std::function<void(char*, size_t)>& cb) {
    auto node = find_node(token);
    if (node) {
        node->set_package_callback(cb);
    }
}

void socket_mgr::set_error_callback(uint32_t token, const std::function<void(const char*)>& cb) {
    auto node = find_node(token);
    if (node) {
        node->set_error_callback(cb);
    }
}

bool socket_mgr::watch_listen(socket_t fd, socket_node* node) {
#ifdef _MSC_VER
    return CreateIoCompletionPort((HANDLE)fd, m_handle, (ULONG_PTR)node, 0) == m_handle;
#endif

#ifdef __linux
    epoll_event ev;
    ev.data.ptr = node;
    ev.events = EPOLLIN | EPOLLET;
    return epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &ev) == 0;
#endif

#ifdef __APPLE__
    struct kevent evt;
    EV_SET(&evt, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, node);
    return kevent(m_handle, &evt, 1, nullptr, 0, nullptr) == 0;
#endif
}

bool socket_mgr::watch_accepted(socket_t fd, socket_node* node) {
#ifdef _MSC_VER
    return CreateIoCompletionPort((HANDLE)fd, m_handle, (ULONG_PTR)node, 0) == m_handle;
#endif

#ifdef __linux
    epoll_event ev;
    ev.data.ptr = node;
    ev.events = EPOLLIN | EPOLLET;
    return epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &ev) == 0;
#endif

#ifdef __APPLE__
    struct kevent evt[2];
    EV_SET(&evt[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, node);
    EV_SET(&evt[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR | EV_DISABLE, 0, 0, node);
    return kevent(m_handle, evt, _countof(evt), nullptr, 0, nullptr) == 0;
#endif
}

bool socket_mgr::watch_connecting(socket_t fd, socket_node* node) {
#ifdef _MSC_VER
    return CreateIoCompletionPort((HANDLE)fd, m_handle, (ULONG_PTR)node, 0) == m_handle;
#endif

#ifdef __linux
    epoll_event ev;
    ev.data.ptr = node;
    ev.events = EPOLLOUT | EPOLLET;
    return epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &ev) == 0;
#endif

#ifdef __APPLE__
    struct kevent evt;
    EV_SET(&evt, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, node);
    return kevent(m_handle, &evt, 1, nullptr, 0, nullptr) == 0;
#endif
}

bool socket_mgr::watch_connected(socket_t fd, socket_node* node) {
#ifdef _MSC_VER
    return true;
#endif

#ifdef __linux
    epoll_event ev;
    ev.data.ptr = node;
    ev.events = EPOLLIN | EPOLLET;
    return epoll_ctl(m_handle, EPOLL_CTL_MOD, fd, &ev) == 0;
#endif

#ifdef __APPLE__
    struct kevent evt[2];
    EV_SET(&evt[0], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, node);
    EV_SET(&evt[1], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR | EV_DISABLE, 0, 0, node);
    return kevent(m_handle, evt, _countof(evt), nullptr, 0, nullptr) == 0;
#endif
}

bool socket_mgr::watch_send(socket_t fd, socket_node* node, bool enable) {
#ifdef _MSC_VER
    return true;
#endif

#ifdef __linux
    epoll_event ev;
    ev.data.ptr = node;
    ev.events = EPOLLIN | (enable ? EPOLLOUT : 0) | EPOLLET;
    return epoll_ctl(m_handle, EPOLL_CTL_MOD, fd, &ev) == 0;
#endif

#ifdef __APPLE__
    struct kevent evt;
    EV_SET(&evt, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR | (enable ? 0 : EV_DISABLE), 0, 0, node);
    return kevent(m_handle, &evt, 1, nullptr, 0, nullptr) == 0;
#endif
}

// 通常情况下,close一个socket后即自动从epoll/kqueue中移除
// 之所以加一个unwatch显式的移除,是为了避免进程fork带来的问题
void socket_mgr::unwatch(socket_t fd) {
#ifdef __linux
    epoll_event ev;
    ev.data.ptr = nullptr;
    ev.events = 0;
    epoll_ctl(m_handle, EPOLL_CTL_DEL, fd, &ev);
#endif

#ifdef __APPLE__
    struct kevent evt[2];
    EV_SET(&evt[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&evt[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(m_handle, evt, _countof(evt), nullptr, 0, nullptr);
#endif
}

int socket_mgr::accept_stream(socket_t fd, const char ip[]) {
    auto token = new_token();    
    auto* stm = new socket_stream(token, this);
    if (watch_accepted(fd, stm) && stm->accept_socket(fd, ip)) {
        m_nodes[token] = stm;
        return token;
    }
    delete stm;
    return 0;
}

socket_node* socket_mgr::find_node(int token) {
    auto it = m_nodes.find(token);
    if (it != m_nodes.end()) {
        return it->second;
    }
    return nullptr;
}

uint32_t socket_mgr::new_token() {
    while (++m_next_token == 0 || m_nodes.find(m_next_token) != m_nodes.end()) {
        // nothing ...
    }
    return m_next_token;
}

