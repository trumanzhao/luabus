/*
** repository: https://github.com/trumanzhao/luabus.git
** trumanzhao, 2017-07-09, trumanzhao@foxmail.com
*/

#include "tools.h"
#include "var_int.h"
#include "lua_socket_node.h"

EXPORT_CLASS_BEGIN(lua_socket_node)
EXPORT_LUA_FUNCTION(call)
EXPORT_LUA_FUNCTION(forward_target)
EXPORT_LUA_FUNCTION_AS(forward_by_group<msg_id::forward_master>, "forward_master")
EXPORT_LUA_FUNCTION_AS(forward_by_group<msg_id::forward_random>, "forward_random")
EXPORT_LUA_FUNCTION_AS(forward_by_group<msg_id::forward_broadcast>, "forward_broadcast")
EXPORT_LUA_FUNCTION(forward_hash)
EXPORT_LUA_FUNCTION(close)
EXPORT_LUA_FUNCTION(set_send_buffer_size)
EXPORT_LUA_FUNCTION(set_recv_buffer_size)
EXPORT_LUA_FUNCTION(set_timeout)
EXPORT_LUA_FUNCTION(set_nodelay)
EXPORT_LUA_STD_STR_AS_R(m_ip, "ip")
EXPORT_LUA_INT_AS_R(m_token, "token")
EXPORT_LUA_BOOL(m_lite_mode)
EXPORT_CLASS_END()

lua_socket_node::lua_socket_node(uint32_t token, lua_State* L, std::shared_ptr<socket_mgr>& mgr,
	std::shared_ptr<lua_archiver>& ar, std::shared_ptr<socket_router> router)
    : m_token(token), m_lvm(L), m_mgr(mgr), m_archiver(ar), m_router(router)
{
    m_mgr->get_remote_ip(m_token, m_ip);

    m_mgr->set_accept_callback(token, [this](uint32_t steam_token)
    {
        lua_guard g(m_lvm);
        auto stream = new lua_socket_node(steam_token, m_lvm, m_mgr, m_archiver, m_router);
        lua_call_object_function(m_lvm, nullptr, this, "on_accept", std::tie(), stream);
    });

    m_mgr->set_connect_callback(token, [this](bool ok, const char* reason)
    {
        if (ok)
        {
            m_mgr->get_remote_ip(m_token, m_ip);
        }

        lua_guard g(m_lvm);
        lua_call_object_function(m_lvm, nullptr, this, "on_connect", std::tie(), ok ? "ok" : reason);
    });

    m_mgr->set_error_callback(token, [this](const char* err)
    {
        lua_guard g(m_lvm);
        lua_call_object_function(m_lvm, nullptr, this, "on_error", std::tie(), err);
    });

    m_mgr->set_package_callback(token, [this](char* data, size_t data_len)
    {
        on_recv(data, data_len);
    });
}

lua_socket_node::~lua_socket_node()
{
	close();
}

int lua_socket_node::call(lua_State* L)
{
    int top = lua_gettop(L);
    if (top < 1)
        return 0;

    size_t data_len = 0;
    void* data = m_archiver->save(&data_len, L, 1, top);
    if (data == nullptr)
        return 0;

    lua_pushinteger(L, data_len);

    if (m_lite_mode)
    {
    	m_mgr->send(m_token, data, data_len);
    	return 1;
    }

    BYTE msg_id_data[MAX_VARINT_SIZE];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::remote_call);
    sendv_item items[] = {{msg_id_data, msg_id_len}, {data, data_len}};
    m_mgr->sendv(m_token, items, _countof(items));
    return 1;
}

int lua_socket_node::forward_target(lua_State* L)
{
    int top = lua_gettop(L);
    if (m_lite_mode || top < 2)
        return 0;

    BYTE msg_id_data[MAX_VARINT_SIZE];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::forward_target);

    uint32_t service_id = (uint32_t)lua_tointeger(L, 1);
    BYTE svr_id_data[MAX_VARINT_SIZE];
    size_t svr_id_len = encode_u64(svr_id_data, sizeof(svr_id_data), service_id);

    size_t data_len = 0;
    void* data = m_archiver->save(&data_len, L, 2, top);
    if (data == nullptr)
        return 0;

    sendv_item items[] = {{msg_id_data, msg_id_len}, {svr_id_data, svr_id_len}, {data, data_len}};
    m_mgr->sendv(m_token, items, _countof(items));
    lua_pushinteger(L, data_len);
    return 1;
}

template <msg_id forward_method>
int lua_socket_node::forward_by_group(lua_State* L)
{
    int top = lua_gettop(L);
    if (m_lite_mode || top < 2)
        return 0;

    static_assert(forward_method == msg_id::forward_master || forward_method == msg_id::forward_random ||
        forward_method == msg_id::forward_broadcast, "Unexpected forward method !");

    BYTE msg_id_data[MAX_VARINT_SIZE];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)forward_method);

    uint8_t group_id = (uint8_t)lua_tointeger(L, 1);
    BYTE group_id_data[MAX_VARINT_SIZE];
    size_t group_id_len = encode_u64(group_id_data, sizeof(group_id_data), group_id);

    size_t data_len = 0;
    void* data = m_archiver->save(&data_len, L, 2, top);
    if (data == nullptr)
        return 0;

    sendv_item items[] = {{msg_id_data, msg_id_len}, {group_id_data, group_id_len}, {data, data_len}};
    m_mgr->sendv(m_token, items, _countof(items));
    lua_pushinteger(L, data_len);
    return 1;
}

// BKDR Hash
static uint32_t string_hash(const char* str)
{
    uint32_t seed = 131; // 31 131 1313 13131 131313 etc..
    uint32_t hash = 0;
    while (*str)
    {
        hash = hash * seed + (*str++);
    }
    return (hash & 0x7FFFFFFF);
}

int lua_socket_node::forward_hash(lua_State* L)
{
    int top = lua_gettop(L);
    if (m_lite_mode || top < 3)
        return 0;

    BYTE msg_id_data[MAX_VARINT_SIZE];
    size_t msg_id_len = encode_u64(msg_id_data, sizeof(msg_id_data), (char)msg_id::forward_hash);

    uint8_t group_id = (uint8_t)lua_tointeger(L, 1);
    BYTE group_id_data[MAX_VARINT_SIZE];
    size_t group_id_len = encode_u64(group_id_data, sizeof(group_id_data), group_id);

    int type = lua_type(L, 2);
    uint32_t hash_key = 0;
    if (type == LUA_TNUMBER)
    {
        hash_key = (uint32_t)lua_tointeger(L, 2);
    }
    else if (type == LUA_TSTRING)
    {
        const char* str = lua_tostring(L, 2);
        if (str == nullptr)
            return 0;
        hash_key = string_hash(str);
    }
    else
    {
        // unexpected hash key
        return 0;
    }

    BYTE hash_data[MAX_VARINT_SIZE];
    size_t hash_len = encode_u64(hash_data, sizeof(hash_data), hash_key);

    size_t data_len = 0;
    void* data = m_archiver->save(&data_len, L, 3, top);
    if (data == nullptr)
        return 0;

    sendv_item items[] = {{msg_id_data, msg_id_len}, {group_id_data, group_id_len}, {hash_data, hash_len}, {data, data_len}};
    m_mgr->sendv(m_token, items, _countof(items));
    lua_pushinteger(L, data_len);
    return 1;
}

void lua_socket_node::close()
{
    if (m_token != 0)
    {
        m_mgr->close(m_token);
        m_token = 0;
    }
}

void lua_socket_node::on_recv(char* data, size_t data_len)
{
	if (m_lite_mode)
	{
		on_call(data, data_len);
		return;
	}

    uint64_t msg = 0;
    size_t len = decode_u64(&msg, (BYTE*)data, data_len);
    if (len == 0)
        return;

    data += len;
    data_len -= len;

    switch ((msg_id)msg)
    {
    case msg_id::remote_call:
        on_call(data, data_len);
        break;

    case msg_id::forward_target:
        m_router->do_forward_target(data, data_len);
        break;

    case msg_id::forward_random:
        m_router->do_forward_random(data, data_len);
        break;

    case msg_id::forward_master:
        m_router->do_forward_master(data, data_len);
        break;

    case msg_id::forward_hash:
        m_router->do_forward_hash(data, data_len);
        break;

    case msg_id::forward_broadcast:
        m_router->do_forward_broadcast(data, data_len);
        break;

    default:
        break;
    }
}

void lua_socket_node::on_call(char* data, size_t data_len)
{
    lua_guard g(m_lvm);

    if (!lua_get_object_function(m_lvm, this, "on_call"))
        return;

    int param_count = m_archiver->load(m_lvm, data, data_len);
    if (param_count == 0)
        return;

    lua_call_function(m_lvm, nullptr, param_count, 0);
}


