/*
** repository: https://github.com/trumanzhao/luabus.git
** trumanzhao, 2017-07-09, trumanzhao@foxmail.com
*/

#pragma once
#include <memory>
#include <array>
#include <vector>
#include "socket_mgr.h"
#include "luna.h"
#include "lua_archiver.h"

struct lua_socket_node final
{
    lua_socket_node(uint32_t token, lua_State* L, std::shared_ptr<socket_mgr>& mgr, std::shared_ptr<lua_archiver>& ar);
    ~lua_socket_node();

    int send(lua_State* L);
    void close();
    void set_send_buffer_size(size_t size) { m_mgr->set_send_buffer_size(m_token, size); }
    void set_recv_buffer_size(size_t size) { m_mgr->set_recv_buffer_size(m_token, size); }
    void set_timeout(int ms) { m_mgr->set_timeout(m_token, ms); }
    void set_nodelay(bool flag) { m_mgr->set_nodelay(m_token, flag); }

private:
    void on_recv(char* data, size_t data_len);

    uint32_t m_token = 0;
    lua_State* m_lvm = nullptr;
    std::string m_ip;
    std::shared_ptr<socket_mgr> m_mgr;
    std::shared_ptr<lua_archiver> m_archiver;

public:
    DECLARE_LUA_CLASS(lua_socket_node);
};

