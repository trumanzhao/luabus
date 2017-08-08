--本文件供除router以外的所有服务进程共同引用
--主要定义了各种rpc工具函数

require("common/service");
require("common/tools");

--master= nil;
routers = routers or {};

function setup(service_name)
    local args = hive.args;

    local group_id = service_groups[service_name];
    if not group_id then
        log_err("undefined service name: %s", service_name);
        os.exit(1);
    end

    hive.id = make_service_id(group_id, args.index or 1);

    local addrs = split_string(args.routers or "127.0.0.1:7070", ";");
    for i, addr in ipairs(addrs) do
        local tab = split_string(addr, ":");
        local ip, port = table.unpack(tab);
        routers[#routers + 1] = {ip=ip, port=port, next_connect_time=0};
    end
end

function connect(node)
    local socket = socket_mgr.connect(node.ip, node.port);
    socket.on_call = function(msg, ...)
        node.alive_time = hive.now;
        if not msg then
            log_err("nil s2s msg !");
            return;
        end

        local proc = s2s[msg];
        if not proc then
            log_err("undefined s2s msg: %s", msg);
            return;
        end

        local ok, err = xpcall(proc, debug.traceback, ...);
        if not ok then
            log_err("failed to call msg s2s.%s", msg);
            log_err("%s", err);
        end
    end

    socket.on_error = function(err)
        node.socket = nil;
        if not node.alive then
            log_err("failed to connect router %s:%s", node.ip, node.port);
            return;
        end
        log_err("router lost %s:%s, err=%s", node.ip, node.port, err);
        node.alive = false;
        switch_master();
    end

    socket.on_connected = function()
        node.alive = true;
        node.alive_time = hive.now;
        socket.call("register", hive.id);
        switch_master();
    end

    node.socket = socket;
end

function switch_master()
    local candidates = {};
    for i, node in pairs(routers) do
        if node.alive then
            candidates[#candidates + 1] = node;
        end
    end

    master = nil;

    local count = #candidates;
    if count > 0 then
        master = candidates[math.random(count)];
        log_info("switch router: %s:%s", master.ip, master.port);
    end
end

_G.call_router = function(msg, ...)
    if master then
        master.socket.call(msg, ...);
    end
end

_G.call_router_all = function(msg, ...)
    for i, node in pairs(routers) do
        if node.alive then        
            node.socket.call(msg, ...);
        end
    end
end

_G.call_target = function(target, msg, ...)
    if master then
        master.socket.forward_target(target, msg, ...);
    end
end

local gateway_group = service_groups.gateway;
_G.call_gateway_all = function(msg, ...)
    if master then
        master.socket.forward_broadcast(gateway_group, msg, ...);
    end
end

local dbagent_group = service_groups.dbagent;
_G.call_dbagent_hash = function(hash_key, msg, ...)
    if master then
        master.socket.forward_hash(dbagent_group, hash_key, msg, ...);
    end
end

local indexsvr_group = service_groups.indexsvr;
_G.call_indexsvr_hash = function(hash_key, msg, ...)
    if master then
        master.socket.forward_hash(indexsvr_group, hash_key, msg, ...);
    end
end

local mailsvr_group = service_groups.mailsvr;
_G.call_mailsvr_hash = function(hash_key, msg, ...)
    if master then
        master.socket.forward_hash(mailsvr_group, hash_key, msg, ...);
    end
end

function s2s.heartbeat()
    --do nothing
end

function update(frame)
    local timeout_value = service_timeout_value;
    for i, node in pairs(routers) do
        if node.alive and hive.now > node.alive_time + timeout_value then
            log_info("router timeout: %s:%s", node.ip, node.port);
            node.alive = false;
            node.socket = nil;
            switch_master();
        end

        if node.socket == nil then
            if hive.now > node.next_connect_time then
                node.next_connect_time = hive.now + 3;
                connect(node);
            end
        end
    end
end

