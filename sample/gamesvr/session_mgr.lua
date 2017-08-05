require("common/tools");

sessions = sessions or {};

function setup()
    local tokens = split_string(hive.args.listen or "127.0.0.1:9001", ":");
    local ip, port = table.unpack(tokens);
    socket = socket_mgr.listen(ip, port);
    if not socket then
        log_err("failed to listen %s:%s", ip, port);
        os.exit(1);
    end
    socket.on_accept = on_connect;
end

function update(frame)
    -- ...
end

next_index = next_index or 1;
function on_connect(node)
    node.index = next_index;
    sessions[next_index] = node; 
    next_index = next_index + 1;

    node.on_call = function(msg, ...)
        node.alive_time = hive.now;
        if not msg then
            log_err("nil c2s msg !");
            return;
        end

        local proc = c2s[msg];
        if not proc then
            log_err("undefined c2s msg: %s", msg);
            return;
        end

        local ok, err = xpcall(proc, debug.traceback, node, ...);
        if not ok then
            log_err("failed to call msg c2s.%s", msg);
            log_err("%s", err);
        end
    end

    node.on_error = function(err)
        log_debug("connection lost, index=%s", node.index);
        sessions[node.index] = nil;
    end
end




