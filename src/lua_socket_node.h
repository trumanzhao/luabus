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
#include "socket_router.h"

// 注意,因为包装层的listener,stream析构的时候,已经close了token
// 所以不存在相关事件(accept, package, error...)触发时,相应的wapper对象失效的问题
// 因为close之后,这些事件不可能触发嘛:)

struct lua_socket_node final
{
    lua_socket_node(uint32_t token, lua_State* L, std::shared_ptr<socket_mgr>& mgr, std::shared_ptr<lua_archiver>& ar, std::shared_ptr<socket_router> router);
    ~lua_socket_node();

    int call(lua_State* L);
    int forward(lua_State* L);
    int forward_target(lua_State* L);

    template <msg_id forward_method>
    int forward_by_group(lua_State* L);

    int forward_hash(lua_State* L);

    void close();
    void set_send_cache(size_t size) { m_mgr->set_send_cache(m_token, size); }
    void set_recv_cache(size_t size) { m_mgr->set_recv_cache(m_token, size); }
    void set_timeout(int duration) { m_mgr->set_timeout(m_token, duration); }

private:
    void on_recv(char* data, size_t data_len);
    void on_call(char* data, size_t data_len);

    uint32_t m_token = 0;
    lua_State* m_lvm = nullptr;
    std::string m_ip;
    std::shared_ptr<socket_mgr> m_mgr;
    std::shared_ptr<lua_archiver> m_archiver;
    std::shared_ptr<socket_router> m_router;

public:
    DECLARE_LUA_CLASS(lua_socket_node);
};

