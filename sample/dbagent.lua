#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
lbus = require("lbus")
lredis = require("lredis");

_G.s2s = s2s or {};

if not hive.init_flag then
    local long_opts = 
    {
        routers=1, --router addr: 127.0.0.1:6000;127.0.0.1:6001
        index=1, --instance index
        log=1, --log file: gamesvr.1
        daemon=0, 
    };
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);

    if args.daemon then
        hive.daemon(1, 1);
    end

    log_open(args.log or "dbagent", 60000);

    hive.print = log_info;
    _G.print = log_debug;
    _G.socket_mgr = lbus.create_socket_mgr(args.connections or 1024);
	_G.redis = lredis.create_agent();

	redis.connect("127.0.0.1", 6379, 1000);

    hive.args = args;
    hive.optind = optind;
    hive.start_time = hive.start_time or hive.get_time_ms();
    hive.frame = hive.frame or 0;

    router_mgr = import("common/router_mgr.lua");
    router_mgr.setup("dbagent");
    hive.init_flag = true;
end

collectgarbage("stop");

import("dbagent/lease_mgr.lua");

redis.on_connect = function(ok)
	log_info("connect redis server ... %s", ok and "ok" or "failed");
	if ok then
		redis.command("set", "rolename:123", "tom");
		redis.command("get", "rolename:123");
	end
end

redis.on_disconnect = function()
	log_err("db connection lost !");
end

redis.on_reply = function(reply)
	log_tree("reply", reply);
end

hive.run = function()
    hive.now = os.time();
    local count = socket_mgr.wait(10) + redis.update();
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
    if frame % 10  == 0 then
        call_router_all("heartbeat", nil);
    end

    router_mgr.update(frame);
end




