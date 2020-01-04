/*
** repository: https://github.com/trumanzhao/luabus.git
** trumanzhao, 2017-07-09, trumanzhao@foxmail.com
*/

#include "tools.h"
#include "var_int.h"
#include "lua_socket_node.h"

LUA_EXPORT_CLASS_BEGIN(lua_socket_node)
LUA_EXPORT_METHOD(send)
LUA_EXPORT_METHOD(close)
LUA_EXPORT_METHOD(set_send_buffer_size)
LUA_EXPORT_METHOD(set_recv_buffer_size)
LUA_EXPORT_METHOD(set_nodelay)
LUA_EXPORT_PROPERTY_AS(m_ip, "ip")
LUA_EXPORT_PROPERTY_READONLY_AS(m_token, "token")
LUA_EXPORT_CLASS_END()

lua_socket_node::lua_socket_node(uint32_t token, lua_State* L, std::shared_ptr<socket_mgr>& mgr, std::shared_ptr<lua_archiver>& ar)
    : m_token(token), m_lvm(L), m_mgr(mgr), m_archiver(ar) {
    m_mgr->get_remote_ip(m_token, m_ip);

    m_mgr->set_accept_callback(token, [this](uint32_t steam_token) {
        lua_guard g(m_lvm);
        auto stream = new lua_socket_node(steam_token, m_lvm, m_mgr, m_archiver);
        lua_call_object_function(m_lvm, nullptr, this, "on_accept", std::tie(), stream);
    });

    m_mgr->set_connect_callback(token, [this](bool ok, const char* reason) {
        if (ok) {
            m_mgr->get_remote_ip(m_token, m_ip);
        }

        lua_guard g(m_lvm);
        lua_call_object_function(m_lvm, nullptr, this, "on_connect", std::tie(), ok ? "ok" : reason);
    });

    m_mgr->set_error_callback(token, [this](const char* err) {
        lua_guard g(m_lvm);
        lua_call_object_function(m_lvm, nullptr, this, "on_error", std::tie(), err);
    });

    m_mgr->set_package_callback(token, [this](char* data, size_t data_len) {
        on_recv(data, data_len);
    });
}

lua_socket_node::~lua_socket_node() {
	close();
}

int lua_socket_node::send(lua_State* L) {
    int top = lua_gettop(L);
    if (top < 1)
        return 0;

    size_t data_len = 0;
    void* data = m_archiver->save(&data_len, L, 1, top);
    if (data == nullptr)
        return 0;
    
    m_mgr->send(m_token, data, data_len);
    lua_pushinteger(L, data_len);
    return 1;
}

void lua_socket_node::close() {
    if (m_token != 0) {
        m_mgr->close(m_token);
        m_token = 0;
    }
}

void lua_socket_node::on_recv(char* data, size_t data_len) {
    lua_guard g(m_lvm);

    if (!lua_get_object_function(m_lvm, this, "on_recv"))
        return;

    int param_count = m_archiver->load(m_lvm, data, data_len);
    if (param_count == 0)
        return;

    lua_call_function(m_lvm, nullptr, param_count, 0);
}


