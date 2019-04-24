#!./hive

lbus = require("lbus")

socket_mgr = lbus.create_socket_mgr(10);

ss = socket_mgr.connect("127.0.0.1", 9999, 2000);

ss.on_connect = function(res)
    print("connect ..."..res);
    ss.ok = true;
end

ss.on_recv = function(msg, ...)
    print("->"..msg);
end

ss.on_error = function(err)
    print(err);
    ss.close();
end

time = hive.get_time_ms();
hive.run = function()
    socket_mgr.wait(100);
    local now = hive.get_time_ms();
    if ss.ok and now > time + 2000 then
        time = now;
        ss.send("hello", 123);
    end
end

