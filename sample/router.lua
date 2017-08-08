#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
require("common/tools");
require("common/service");
lbus = require("lbus");

_G.s2s = s2s or {};
sessions = sessions or {};
groups = groups or {};

if not hive.init_flag then
    local long_opts = 
    {
        listen=1, --listen addr for servers: 127.0.0.1:7070
        index=1, --instance index
        daemon=0, 
        log=1, --log file: router.1
        connections=1, --max-connection-count
    };
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);

    if args.daemon then
        hive.daemon(1, 1);
    end

    log_open(args.log or "router", 60000);

    hive.print = log_info;
    _G.print = log_debug;
    socket_mgr = lbus.create_socket_mgr(args.connections or 64);

    hive.args = args;
    hive.optind = optind;
    hive.start_time = hive.start_time or hive.get_time_ms();
    hive.frame = hive.frame or 0;

    local addr = args.listen or "127.0.0.1:7070";
    local tokens = split_string(addr, ":");
    local ip, port = table.unpack(tokens);
    listener = socket_mgr.listen(ip, port);
    if not listener then
        log_err("failed to listen at %s", addr);
        os.exit(1);
    end

    hive.init_flag = true;
end

listener.on_accept = function(ss)
    log_info("new connection, token=%s", ss.token);

    ss.set_timeout(1000 * service_timeout_value);
    sessions[ss.token] = ss;

    ss.on_call = function(msg, ...)
        if not msg then
            log_err("nil s2s msg !");
            return;
        end

        local proc = s2s[msg];
        if not proc then
            log_err("undefined s2s msg: %s", msg);
            return;
        end

        local ok, err = xpcall(proc, debug.traceback, ss, ...);
        if not ok then
            log_err("failed to call s2s msg: %s", msg);
            log_err(err);
        end
    end

    ss.on_error = function(err)
        log_err("%s lost: %s", ss.name or ss.token, err);
        if ss.id then
            --要实现固定哈希的话,可以把这里的nil改为0
            socket_mgr.map_token(ss.id, nil);
            local idx = get_service_group(ss.id);
            local group = groups[idx]; 
            if group and ss.token == group.master then
                group.master = 0;
                group.lease_time = 0;
                socket_mgr.set_master(idx, 0);
                log_info("switch master %s --> nil", ss.name);
            end
        end
        sessions[ss.token] = nil;
    end
end

function s2s.register(ss, id)
    if not ss.id then
        ss.id = id;
        ss.name = service_id2name(id);
        socket_mgr.map_token(id, ss.token);
        log_info("register service: %s", ss.name);
    end
end

hive.start_time = hive.start_time or hive.get_time_ms();
hive.frame = hive.frame or 0;

collectgarbage("stop");

--逻辑帧间隔: 100毫秒,即10帧每秒
function hive.run()
    hive.now = os.time();

    local msg_count = socket_mgr.wait(10);
    local cost_time = hive.get_time_ms() - hive.start_time;
    if 100 * hive.frame <  cost_time  then
        hive.frame = hive.frame + 1;
        local ok, err = xpcall(on_tick, debug.traceback, hive.frame);
        if not ok then
            log_err("on_tick error: %s", err);
        end
        collectgarbage("collect");
    end

    if check_quit_signal() then
        log_info("service quit for signal !");
        hive.run = nil;
    end
end


function on_tick(frame)
    --nothing
end

--lease_time: master租约时间戳,可以为nil
function s2s.heartbeat(ss, lease_time)
    local idx = get_service_group(ss.id);
    local group = groups[idx] or {lease_time=0, master=0, name=service_names[idx]};

    groups[idx] = group;

    ss.call("heartbeat");

    if lease_time and lease_time > group.lease_time then
        group.lease_time = lease_time;
        if ss.token ~= group.master then
            log_info("switch master %s --> %s", service_id2name(group.master), ss.name);
            group.master = ss.token;
            socket_mgr.set_master(idx, ss.token);
        end
    end

    if ss.token == group.master and not lease_time then
        group.master = 0;
        group.lease_time = 0;
        socket_mgr.set_master(idx, 0);
        log_info("switch master %s --> nil", ss.name);
    end
end

