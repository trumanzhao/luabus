/*
** repository: https://github.com/trumanzhao/luabus.git
** trumanzhao, 2017-07-09, trumanzhao@foxmail.com
*/

#include "tools.h"
#include "var_int.h"
#include "lua_socket_mgr.h"
#include "lua_socket_node.h"

LUA_EXPORT_CLASS_BEGIN(lua_socket_mgr)
LUA_EXPORT_METHOD(wait)
LUA_EXPORT_METHOD(listen)
LUA_EXPORT_METHOD(connect)
LUA_EXPORT_METHOD(set_package_size)
LUA_EXPORT_METHOD(set_lz_threshold)
LUA_EXPORT_CLASS_END()

lua_socket_mgr::~lua_socket_mgr() {
}

bool lua_socket_mgr::setup(lua_State* L, int max_fd) {
    m_lvm = L;
    m_mgr = std::make_shared<socket_mgr>();
    m_archiver = std::make_shared<lua_archiver>(1024);
    return m_mgr->setup(max_fd);
}

int lua_socket_mgr::listen(lua_State* L) {
    const char* ip = lua_tostring(L, 1);
    int port = (int)lua_tointeger(L, 2);
    if (ip == nullptr || port <= 0) {
        lua_pushnil(L);
        lua_pushstring(L, "invalid param");
        return 2;
    }

    std::string err;
    int token = m_mgr->listen(err, ip, port);
    if (token == 0) {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }

    auto listener = new lua_socket_node(token, m_lvm, m_mgr, m_archiver);
    lua_push_object(L, listener);
    lua_pushstring(L, "ok");
    return 2;
}

int lua_socket_mgr::connect(lua_State* L) {
    const char* ip = lua_tostring(L, 1);
    const char* port = lua_tostring(L, 2);
    int timeout = (int)lua_tonumber(L, 3);
    if (ip == nullptr || port == nullptr) {
        lua_pushnil(L);
        lua_pushstring(L, "invalid param");
        return 2;
    }

    std::string err;
    int token = m_mgr->connect(err, ip, port, timeout);
    if (token == 0) {
        lua_pushnil(L);
        lua_pushstring(L, err.c_str());
        return 2;
    }

    auto stream = new lua_socket_node(token, m_lvm, m_mgr, m_archiver);
    lua_push_object(L, stream);
    lua_pushstring(L, "ok");
    return 2;
}

void lua_socket_mgr::set_package_size(size_t size) {
    m_archiver->set_buffer_size(size);
}

void lua_socket_mgr::set_lz_threshold(size_t size) {
    m_archiver->set_lz_threshold(size);
}

