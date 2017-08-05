#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
lbus = require("lbus");

_G.s2s = s2s or {};

if not hive.init_flag then
    local long_opts = 
    {
        listen=1, --listen addr for servers: 127.0.0.1:5000
        index=1, --instance index
        daemon=0, 
        log=1, --log file: router.1
        connections=1, --max-connection-count
    };
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);

    hive.print = print;
    if args.daemon then
        hive.daemon(1, 1);
        _G.print = log_debug;
        hive.print = log_info;
    end

    log_open(args.log or "router", 60000);

    socket_mgr = lbus.create_socket_mgr(args.connections or 64);

    hive.args = args;
    hive.optind = optind;
    hive.start_time = hive.start_time or hive.get_time_ms();
    hive.frame = hive.frame or 0;

    local addr = args.listen or "127.0.0.1:6000";
    local tokens = split_string(addr, ":");
    local ip, port = table.unpack(tokens);
    listener = socket_mgr.listen(ip, port);
    if not listener then
        log_err("failed to listen at %s", addr);
        os.exit(1);
    end

    hive.init_flag = true;
end

sessions = sessions or {};

listener.on_accept = function(ss)
    log_info("new connection ...");

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
        sessions[ss.token] = nil;
        log_err("connection lost: %s", err);
    end
end

function s2s.register(ss, id)

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
end

--lease_time: master租约时间戳,可以为nil
function s2s.on_heartbeat(ss, lease_time)
    if not ss.id then
        return;
    end

    local group_idx = get_service_group(ss.id);
    local group = groups[group_idx];
    local route_changed = false;

    ss.call("on_heartbeat");

    if lease_time and lease_time > group.lease_time then
        group.lease_time = lease_time;
        if ss.id ~= group.master then
            log_info("switch master %s: %s --> %s", group.name, service_id2name(group.master), ss.name);
            group.master = ss.id;
            route_changed = true;
        end
    end

    if ss.id == group.master and not lease_time then
        group.master = 0;
        group.lease_time = 0;
        log_info("switch master %s: %s --> nil", group.name, ss.name);
    end

    if route_changed then
        update_route_table(group_idx, group);
    end
end

