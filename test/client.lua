#!./hive

lbus = require("lbus")

socket_mgr = lbus.create_socket_mgr(10000);

sessions = {};

session_count = 0;
message_count = 0;
error_count = 0;
errors = {};

function init_session(ss)
    ss.count = 0;
    ss.on_connect = function(res)
        if res == "ok" then
            session_count = session_count + 1; 
            ss.send("hello", 123);            
            return;
        end
        sessions[ss.token] = nil;        
    end

    ss.on_recv = function(msg, ...)
        ss.send(msg, ...);
        ss.count = ss.count + 1;
        message_count = message_count + 1;

        if ss.count > 100 then
            sessions[ss.token] = nil;
            session_count = session_count - 1;
            ss.close();        
        end
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
end

print_time = hive.get_time_ms();
hive.run = function()
    socket_mgr.wait(100);

    local now = hive.get_time_ms();
    if now > print_time then
        print("session_count="..session_count..", message_count="..message_count..", error_count="..error_count);
        print_time = now + 2000;
    end

    local ss = socket_mgr.connect("127.0.0.1", 9999, 2000);
    if ss then
        init_session(ss);
    end
end

