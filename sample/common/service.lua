--每个服务进程都有一个唯一的服务标识,由服务分组(group)和服务索引(index)两部分构成
--有三种形式:
--servcie_id(string): 2.1
--service_id(number): 131073
--service_name: gamesvr.1
--在上面的示例中,服务id 2.1中的2表明服务分组(group)为2(gamesvr),实例编号(index)为1

service_groups = 
{
    router = 1,
    gamesvr = 2,
    gateway = 3,
    dbagent = 4,
    indexsvr = 5,
    mailsvr = 6,
};

service_names =
{
    [service_groups.router] = "router",
    [service_groups.gamsvr] = "gamesvr",
    [service_groups.gateway] = "gateway",
    [service_groups.dbagent] = "dbagent",
    [service_groups.indexsvr] = "indexsvr",
    [service_groups.mailsvr] = "mailsvr",
};

service_timeout_value = 5; --通道超时时间,同时也是master租约超时时间

function make_service_id(group, index)
    return (group << 16) | index;
end

function get_service_group(id)
    return id >> 16;
end

function get_service_index(id)
    return id & 0xff;
end

function service_id2str(id)
    local group = id >> 16;
    local index = id & 0xff;
    local fmt = "%s.%s";
    return fmt:format(group, index);
end

function service_str2id(str)
    local pos = str:find(".");
    local group = str:sub(1, pos - 1);
    local index = str.sub(pos + 1, #str);
    return make_service_id(tonumber(group), tonumber(index));
end

function service_id2name(id)
    local group = id >> 16;
    local index = id & 0xff;
    local fmt = "%s.%s";
    return fmt:format(service_names[group], index);
end



