#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/signal");
base = require("base");
tcaplus = require("tcaplus");

_G.s2s = s2s or {};

if not hive.init_flag then
    local long_opts = {tbus=1, id=1, daemon=0, log=1};
    local args, optind = alt_getopt.get_opts(hive.args, "", long_opts);
    if not args.tbus then
        print("--tbus=KEY required !");
        os.exit(1);
    end

    if not args.id then
        print("--id=ID required !");
        os.exit(1);
    end

    if args.daemon then
        hive.daemon(1, 1);
    end

    log_open(args.log or "dbagent", 60000);

    local db = tcaplus.create_dbagent("app_id", "zone_id", "sig", {"dir1", "dir2", "dir3"}); 
    if not db then
        log_err("failed to connect tcaplus !");
        os.exit(1);
    end

    db.callbacks = {};

    _G.dbagent = db;
    _G.print = log_debug;
    hive.print = log_info;
    hive.args = args;
    hive.optind = optind;
    hive.init_flag = true;
end

dbagent.on_complete = function(token, ...)
    local proc = dbagent.callbacks[token];
    if proc then
        dbagent.callbacks[token] = nil;
        proc(...);
    end
end

router_mgr = import("common/router_mgr.lua");
------ 其他需要import的模块放在这下面 ----------


hive.start_time = hive.start_time or hive.get_time_ms();
hive.frame = hive.frame or 0;

collectgarbage("stop");
call_router_all("on_heartbeat", nil); --立即激活所有router

hive.run = function()
    hive.now = os.time();

    local count = busmgr.update() + dbagent.update();
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

    local now = os.time();
    for key, queue in pairs(failover_queues) do
        if #queue > 0 and now > queue.lock + 5 then
            queue.lock = now;
            try_failover(queue);
        end
    end
end

failover_queues = failover_queues or {};
function s2s.on_failover_master(key, id)
    local queue = failover_queues[key] or {lock=0};
    local node = {key=key, id=id};
    queue[#queue + 1] = node;
    failover_queues[key] = queue;    
end

function try_failover(queue)
    local node = table.remove(queue, 1);
    local key = node.key;
    local id = node.id;
    local token = dbagent.load_custom_data(key);
    dbagent.callbacks[token] = function(ok, data)
        queue.lock = 0;
        if not ok then
            log_err("failed to load custom data: %s", key);
            return;
        end

        local now = os.time();
        data = data or {id=0, time=0};
        if now > data.time + tbus_channel_timeout_value or id == data.id then
            data.id = id;
            data.time = now;
            dbagent.save_custom_data(key, data);
            call_target(id, "on_failover_master", now);
        else
            call_target(id, "on_failover_master", nil);
        end
    end
end





