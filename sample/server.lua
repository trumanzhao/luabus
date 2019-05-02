#!./hive

lbus = require("lbus")

socket_mgr = lbus.create_socket_mgr(10000);
sessions = {};

listener = socket_mgr.listen("127.0.0.1", 9999);

session_count = 0;
message_count = 0;
message_speed = 0;
error_count = 0;
errors = {};

listener.on_accept = function(ss)
    ss.on_recv = function(msg, ...)
        ss.send(msg, ...);
        message_count = message_count + 1;
        message_speed = message_speed + 1;
    end

    ss.on_error = function(err)
        sessions[ss.token] = nil;
        session_count = session_count - 1;
        error_count = error_count + 1;
        if errors[err] == nil then
            errors[err] = 1;
            print(err);
        end
    end

    sessions[ss.token] = ss;
    session_count = session_count + 1;
end

print_time = hive.get_time_ms();

hive.run = function()
    socket_mgr.wait(100);

    local now = hive.get_time_ms();
    if now > print_time then
        local speed = message_speed // 2;
        print("session_count="..session_count..", message_count="..message_count..", message_speed="..speed);
        print_time = now + 2000;
        message_speed = 0;
    end
end

