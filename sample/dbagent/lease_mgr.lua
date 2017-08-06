--服务进程master租约管理逻辑

lease_table = lease_table or {};

--进程申请获取租约或者续约
--id: 请求进程的service id
function s2s.on_failover_req(ss, id)
    local idx = get_service_group(id);    
    local key = service_names[idx];
    if not key then
        log_err("undefined service group !");
        return;
    end

    --注意,正式设计中,这个租约记录应该是存数据库的
    --而租约操作应该是一个CAS操作或者是一个数据库事务
    --这里作为一个demo,就不实际操作数据库了
    local lease = lease_table[key] or {master=id, time=0};
    lease_table[key] = lease;
    
    local now = os.time();
    if lease.master == id or now > lease.time + service_timeout_value then
        lease.master = id;
        lease.time = now;
        call_target(id, "on_failover_res", now);
        return;
    end
    call_target(id, "on_failover_res", nil);
end



