#!./hive

lbus = require("lbus")

socket_mgr = lbus.create_socket_mgr(1024);

listen_node = socket_mgr.listen("127.0.0.1", 9999);

sessions = {};

listen_node.on_accept = function(ss)
    ss.on_recv = function(msg, ...)
        print("recv:" .. msg);
        ss.send("recved", msg, ...);
    end

    ss.on_error = function(err)
        print(err);
        sessions[ss.token] = nil;
    end

    sessions[ss.token] = ss;
end

hive.run = function()
    socket_mgr.wait(100);
end

