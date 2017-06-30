﻿/*
** repository: https://github.com/trumanzhao/luabus
** trumanzhao, 2017-06-25, trumanzhao@foxmail.com
*/

#include "socket_wapper.h"

int create_socket_mgr(lua_State* L)
{
    int max_fd = (int)lua_tonumber(L, 1);
    lua_socket_mgr* mgr = new lua_socket_mgr();
    if (!mgr->setup(L, max_fd))
    {
        delete mgr;
        lua_pushnil(L);
        return 1;
    }
    lua_push_object(L, mgr);
    return 1;
}

extern "C" int luaopen_lbus(lua_State* L)
{
    lua_newtable(L);
    lua_set_table_function(L, -1, "create_socket_mgr", create_socket_mgr);
    return 1;
}

