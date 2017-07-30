#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
robotlib = require("robot");

_G.s2c = s2c or {};

function printf(fmt, ...)
    print(fmt:format(...));
end

svr_url = "tcp://127.0.0.1:7572?reuse=1";

if not hive.init_flag then
    robot = robotlib.create_robot("1000001286", svr_url, 2, 1000, 1024 * 1024);
    if not robot then
        print("failed to create robot !");
        os.exit(1);
    end
    hive.init_flag = true;
end

------ 其他需要import的模块放在这下面 ----------


hive.start_time = hive.start_time or hive.get_time_ms();
hive.frame = hive.frame or 0;

collectgarbage("stop");

hive.run = function()
    hive.now = os.time();

    robot.update(10);

    local cost_time = hive.get_time_ms() - hive.start_time;
    if 100 * hive.frame <  cost_time  then
        hive.frame = hive.frame + 1;
        local ok, err = xpcall(on_tick, debug.traceback, hive.frame);
        if not ok then
            printf("on_tick error: %s", err);
        end
        collectgarbage("collect");
    end

    if check_quit_signal() then
        print("quit for signal !");
        hive.run = nil;
    end
end

sessions = sessions or {};

function do_connect(atk, openid, timeout)
    local idx = robot.connect(atk, openid, timeout);
    if not idx then
        print("connect err, bad param ???");
        return;
    end
    sessions[idx] = {openid=openid, atk=atk};
end


robot.on_connect = function(conn_idx, code)
    if code ~= "ok" then
        sessions[conn_idx] = nil;
        printf("failed to connnect svr %s, err=%s", svr_url, code);
        return;
    end
    robot.call(conn_idx, "hello", "I'm robot !");
end

robot.on_disconnect = function(conn_idx)
    printf("connection lost: %d", conn_idx);
    sessions[conn_idx] = nil;
end

robot.on_call = function(conn_idx, msg, ...)
    local ss = sessions[conn_idx];
    if not ss then
        printf("msg %s with nil session, conn_idx=%d", msg, conn_idx);
        return;
    end

    local proc = s2c[msg];
    if not proc then
        printf("undefined msg %s, conn_idx=%d", msg, conn_idx);
        return;
    end

    proc(ss, ...);
end

function on_tick(frame)
end


do_connect("myatk", "id123", 1000);

function s2c.welcome(ss, txt)
    printf("welcome %s: %s", ss.openid, txt);
end



