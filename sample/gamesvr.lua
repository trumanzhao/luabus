#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
lbus = require("lbus");
router_mgr = import("common/router_mgr.lua");
session_mgr = import("lobby/session_mgr.lua");

_G.s2s = s2s or {};
_G.c2s = c2s or {};

if not hive.init_flag then
    local long_opts = {routers=1, listen=1, index=1, daemon=0, connections=1};
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);

    if args.daemon then
        hive.daemon(1, 1);
    end

    log_open(args.log or "gamesvr", 60000);

    _G.print = log_debug;
    _G.socket_mgr = lbus.create_socket_mgr(args.connections or 16);

    hive.print = log_info;
    hive.args = args;
    hive.optind = optind;
    hive.start_time = hive.start_time or hive.get_time_ms();
    hive.frame = hive.frame or 0;

    router_mgr.setup("gamesvr");
    session_mgr.setup();

    hive.init_flag = true;
end

collectgarbage("stop");

hive.run = function()
    hive.now = os.time();

    local count = socket_mgr.wait();
    local cost_time = hive.get_time_ms() - hive.start_time;
    if 100 * hive.frame <  cost_time  then
        hive.frame = hive.frame + 1;
        local ok, err = xpcall(on_tick, debug.traceback, hive.frame);
        if not ok then
            log_err("on_tick error: %s", err);
        end
        collectgarbage("collect");
    elseif count == 0 then
        hive.sleep_ms(5);
    end

    if check_quit_signal() then
        log_info("service quit for signal !");
        hive.run = nil;
    end
end

function on_tick(frame)
    if frame % 10  == 0 then
        call_router_all("on_heartbeat", nil);
    end

    router_mgr.update(frame);
    session_mgr.update(frame);
end

function c2s.hello(ss, txt)
    log_debug("hello %s, from %s", txt, ss.openid);
    call_client(ss.conn_idx, "welcome", "I'm god !");
end



