#!./hive
require("common/log");
require("common/tree");
require("common/alt_getopt");
require("common/tbus_id");
require("common/signal");
base = require("base");

_G.s2s = s2s or {};

function do_init()
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

    log_open(args.log or "router", 60000);

    _G.print = log_debug;
    hive.print = log_info;
    hive.args = args;
    hive.optind = optind;

    if not base.tbus_init(args.tbus) then
        log_err("failed to init tbus with key=%s", hive.tbus);
        os.exit(1);
    end

    hive.id = base.tbus_aton(args.id);

    local mgr = base.create_tbus_mgr(hive.id, 1024 * 1024 * 16);
    if not mgr then
        log_err("failed to create tbus mgr with id=%s", base.tbus_ntoa(hive.id));
        os.exit(1);
    end
    _G.busmgr = mgr;

    channels = {};
    groups = {};

    local dst_list = busmgr.get_dst_list();
    for i, dst in ipairs(dst_list) do
        if dst == 0 then
            log_err("bad dst id: %s", base.tbus_ntoa(dst));
            os.exit(1);
        end

        if channels[dst] then
            log_err("duplicated dst id: %s", base.tbus_ntoa(dst));
            os.exit(1);
        end

        local dst_group = tbus_get_group_id(dst);
        local channel = busmgr.create_channel(dst);

        channel.id = dst;
        channel.name = base.tbus_ntoa(dst);
        channel.service = service_names[dst_group];
        channel.on_call = function(msg, ...)
            if not msg then
                log_err("nil s2s msg !");
                return;
            end

            local proc = s2s[msg];
            if not proc then
                log_err("undefined s2s msg %s from %s", msg, channel.name);
                return;
            end

            local ok, err = xpcall(proc, debug.traceback, channel, ...);
            if not ok then
                log_err("failed to call s2s msg: %s", msg);
                log_err(err);
            end
        end

        channels[dst] = channel;

        log_info("load channel %s<%s>", channel.service, channel.name);

        --每个group存放了几项数据:
        --master: 主进程的id,用于设置路由
        --lease_time: master租约时间戳
        --ids: 该分组下的所有id列表,从小到大排序(这是为了保证所有router哈希一致性),它用于设置路由的时候保证顺序(路由表临时构建)
        --name: 服务名称
        local group = groups[dst_group] or {master=0, lease_time=0, ids={}, name=service_names[dst_group]};
        group.ids[#group.ids + 1] = dst;
        groups[dst_group] = group;
    end

    --对group的ids排序:
    for idx, group in pairs(groups) do
        table.sort(group.ids);
    end
end

if not hive.init_flag then
    do_init();
    hive.init_flag = true;
end

hive.start_time = hive.start_time or hive.get_time_ms();
hive.frame = hive.frame or 0;

collectgarbage("stop");

--逻辑帧间隔: 100毫秒,即10帧每秒
function hive.run()
    hive.now = os.time();

    local msg_count = busmgr.update();
    local cost_time = hive.get_time_ms() - hive.start_time;
    if 100 * hive.frame <  cost_time  then
        hive.frame = hive.frame + 1;
        local ok, err = xpcall(on_tick, debug.traceback, hive.frame);
        if not ok then
            log_err("on_tick error: %s", err);
        end
        collectgarbage("collect");
    elseif msg_count == 0 then
        hive.sleep_ms(5);
    end

    if check_quit_signal() then
        log_info("service quit for signal !");
        hive.run = nil;
    end
end

function on_tick(frame)
    local timeout_value = tbus_channel_timeout_value;
    for group_idx, group in pairs(groups) do
        for i, id in ipairs(group.ids) do
            local channel = channels[id];
            if channel.alive and hive.now > channel.alive_time + timeout_value then
                channel.alive = false;
                log_info("channel timeout %s<%s>", channel.service, channel.name);
                if id == group.master then
                    group.master = 0;
                    group.lease_time = 0;
                    log_info("switch master %s: %s --> %s", group.name, channel.name, base.tbus_ntoa(0));
                end
                update_route_table(group_idx, group);
            end
        end
    end
end

function update_route_table(group_idx, group)
    local route_table = {};
    for _, id in ipairs(group.ids) do
        local channel = channels[id];
        route_table[#route_table + 1] = channel.alive and id or 0;
    end
    busmgr.route(group_idx, group.master, route_table);
end

--lease_time: master租约时间戳,可以为nil
function s2s.on_heartbeat(channel, lease_time)
    local group_idx = tbus_get_group_id(channel.id);
    local group = groups[group_idx];
    local route_changed = false;

    channel.alive_time = hive.now;
    channel.call("on_heartbeat");

    if not channel.alive then
        log_info("channel alive %s<%s>", channel.service, channel.name);
        channel.alive = true;
        route_changed = true;
    end

    if lease_time and lease_time > group.lease_time then
        group.lease_time = lease_time;
        if channel.id ~= group.master then
            log_info("switch master %s: %s --> %s", group.name, base.tbus_ntoa(group.master), channel.name);
            group.master = channel.id;
            route_changed = true;
        end
    end

    if channel.id == group.master and not lease_time then
        group.master = 0;
        group.lease_time = 0;
        log_info("switch master %s: %s --> %s", group.name, channel.name, base.tbus_ntoa(0));
    end

    if route_changed then
        update_route_table(group_idx, group);
    end
end

