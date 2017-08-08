require("common/tools");

sessions = sessions or {};
session_count = session_count or 0;

function setup()
    local tokens = split_string(hive.args.listen or "127.0.0.1:7572", ":");
    local ip, port = table.unpack(tokens);
    listener = socket_mgr.listen(ip, port);
    if not listener then
        log_err("failed to listen %s:%s", ip, port);
        os.exit(1);
    end
	log_info("listen client at %s:%s", ip, port);
	listen_ip = ip;
	listen_port = port;
    listener.on_accept = on_accept;
end

function update(frame)
	if frame % 10 == 0 then
		call_gateway_all("sync_payload", hive.id, session_count, listen_ip, listen_port);
	end
end

function on_accept(ss)
    sessions[ss.token] = ss; 
	session_count = session_count + 1;

    ss.on_call = function(msg, ...)
        ss.alive_time = hive.now;
        if not msg then
            log_err("nil c2s msg !");
            return;
        end

        local proc = c2s[msg];
        if not proc then
            log_err("undefined c2s msg: %s", msg);
            return;
        end

        local ok, err = xpcall(proc, debug.traceback, ss, ...);
        if not ok then
            log_err("failed to call msg c2s.%s", msg);
            log_err("%s", err);
        end
    end

    ss.on_error = function(err)
        sessions[ss.token] = nil;
		session_count = session_count - 1;
        log_debug("connection lost, token=%s", ss.token);
    end

	log_info("new connection, token=%s", ss.token);
end




