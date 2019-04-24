# `luabus`

luabus是一个为LUA提供序列化消息传输的简易网络库.

## 编译环境

目前支持Windows, Linux, MacOS三平台,编译器必须支持C++17.

- Windows: Visual studio 2017以上版本,需要自行编译lua的dll库.
- MacOS: 需要自行编译安装lua.
- Linux: 需要自行编译安装lua.

## 性能指标

目前尚未专门为性能做测试及优化,只是为了简单好用.  

## 网络
首先需要一个socket\_mgr对象:

```lua
lbus = require("lbus");
socket_mgr = lbus.create_socket_mgr(最大句柄数);
--用户需要在主循环中调用wait函数,比如:
--其参数为最大阻塞时长(实际是传给epoll_wait之类函数),单位ms.
socket_mgr.wait(50);
```

监听端口:

```lua
--注意保持listener的生存期,一旦被gc,则端口就自动关了
listener = mgr.listen("127.0.0.1", 8080);

--设置accept回调
listener.on_accept = function(stream)
    stream.on_recv = function(msg, ...)
        -- ...
    end

    stream.on_error = function(err)
        -- ...
    end
end
```

发起连接:

```lua
--注意保持stream对象生存期,被gc的话,连接会自动关闭
--连接都是异步进行的,一直要stream上面触发'on_connect'事件后,连接才可用
--connect(ip, port, timeout)
stream = mgr.connect("127.0.0.1", 8080, 2000);
--设置连接事件回调:
stream.on_connect = function(result)
    --如果连接成功,result为"ok"
    --否则,result表明具体的错误
end
```

向对端发送消息:

```lua
stream.send("on_login", acc, password);
```

响应消息:

```lua
stream.on_error = function (err)
    --发生任何错误时触发
end

stream.on_recv = function (msg, ...)
    --收到对端消息时触发.
    --通常这里是以...为参数调用msg对应的函数过程.
end
```

其他方法:
```lua
--设置多久没收到消息就超时,默认不超时
stream.set_timeout(2000);
--设置nodelay属性,默认设置为true
stream.set_nodelay(true);
--设置收发缓冲区,至少应该可以容纳一条消息,默认64K
stream.set_recv_buffer_size(1024 * 64);
stream.set_send_buffer_size(1024 * 64);
stream.ip: 对端ip
stream.token: 本连接的唯一标识
```

主动断开:

```lua
--调用close即可:
stream.close();
```
