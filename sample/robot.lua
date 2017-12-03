#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
require("common/tools");
lbus = require("lbus")

_G.s2c = s2c or {};

if not hive.init_flag then
    local long_opts =
    {
        server=1, --127.0.0.1:7571
        openid=1, --openid
        log=1,
    };
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);

    log_open(args.log or "robot", 60000);

    hive.print = log_info;
    _G.print = log_debug;
    socket_mgr = lbus.create_socket_mgr(1024);

    hive.args = args;
    hive.optind = optind;
    hive.start_time = hive.start_time or hive.get_time_ms();
    hive.frame = hive.frame or 0;
    hive.init_flag = true;
end

function do_connect(addr)
	local tokens = split_string(addr, ":");
	local ip, port = table.unpack(tokens);

    log_debug("try connect: %s", addr);

	socket = socket_mgr.connect(ip, port);
    socket.on_call = function(msg, ...)
        if not msg then
            log_err("nil s2c msg !");
            return;
        end

        local proc = s2c[msg];
        if not proc then
            log_err("undefined s2c msg: %s", msg);
            return;
        end

        local ok, err = xpcall(proc, debug.traceback, ...);
        if not ok then
            log_err("failed to call msg s2c.%s", msg);
            log_err("%s", err);
        end
    end

    socket.on_error = function(err)
        log_err("connection lost, err=%s", err);
        os.exit(1);
    end

    socket.on_connect = function()
        log_info("connect ok !");
        socket.call("login_req");
    end
end

if not socket then
    do_connect(hive.args.server or "127.0.0.1:7571");
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
        log_info("robot quit for signal !");
        hive.run = nil;
    end
end

function on_tick(frame)
end

function s2c.login_res(res, hotfix, ip, port)
    log_info("login_res=%s", res);
    log_info("hotfix=%s", hotfix);
    log_info("ip=%s,port=%s", ip, port);
end

