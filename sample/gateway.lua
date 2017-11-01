#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
lbus = require("lbus")

_G.s2s = s2s or {};
_G.c2s = c2s or {};

if not hive.init_flag then
    local long_opts = 
    {
        routers=1, --router addr: 127.0.0.1:6000;127.0.0.1:6001
        listen=1, --listen addr for client: 127.0.0.1:5000
        index=1, --instance index
        daemon=0, 
        log=1, --log file: gamesvr.1
        connections=1, --max-connection-count
    };
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);

    if args.daemon then
        hive.daemon(1, 1);
    end

    log_open(args.log or "gateway", 60000);

    hive.print = log_info;
    _G.print = log_debug;
    _G.socket_mgr = lbus.create_socket_mgr(args.connections or 1024);

    hive.args = args;
    hive.optind = optind;
    hive.start_time = hive.start_time or hive.get_time_ms();
    hive.frame = hive.frame or 0;

    router_mgr = import("common/router_mgr.lua");
    session_mgr = import("gateway/session_mgr.lua");

    router_mgr.setup("gateway");
    session_mgr.setup();

    hive.init_flag = true;
end

collectgarbage("stop");

hive.run = function()
    hive.now = os.time();

    local count = socket_mgr.wait(10);
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
    session_mgr.update(frame);
end

lobby_list = lobby_list or {};

function s2s.sync_payload(id, payload, ip, port)
	local node = lobby_list[id] or {};
	node.payload = payload;
	node.ip = ip;
	node.port = port;
	node.time = os.time();
	lobby_list[id] = node;
	log_debug("sync payload, id=%s, payload=%s, url=%s:%s", service_id2name(id), payload, ip, port);
end

function find_best_lobby()
	local sel = nil;
	local now = os.time();
	for id, node in pairs(lobby_list) do
		if node.time > now - service_timeout_value then
			if sel == nil or node.payload < sel.payload then
				sel = node;
			end
		end
	end
	return node;
end

local hotfix = "print('exec some code')";

function c2s.login_req(ss)
	local node = find_best_lobby();
	if not node then
		ss.call("login_res", "not-open", hotfix);
		return;
	end
	node.payload = node.payload + 1;
	ss.call("login_res", "ok", hotfix, node.ip, node.port);
end

function s2s.test(svr, param)
    call_target(svr, "test", param);
end


