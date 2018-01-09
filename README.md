# `luabus`

luabus是一个为LUA RPC服务集群提供网络支持的基础库,主要分为网络库和路由支持两块功能.

主要特性:
- 方便的在服务进程间进行RPC调用.
- 方便搭建高可用集群,支持: 定点发送,哈希转发,随机转发,组内广播,主从备份.

## 编译环境

目前支持Windows, Linux, MacOS三平台,编译器必须支持C++14.

- Windows: Visual studio 2015以上版本,需要自行编译lua的dll库,或者用win64目录下的库文件.
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
    stream.on_call = function(msg, ...)
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
stream.call("on_login", acc, password);
```

响应消息:

```lua
stream.on_error = function (err)
    --发生任何错误时触发
end

stream.on_call = function (msg, ...)
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
```

主动断开:

```lua
--调用close即可:
stream.close();
```

## 路由转发

路由转发目前针对星型架构设计,它以转发器(router)为中心,构成一个高可用集群,其中的所有服务进程(包括router)都被设计成多活的.

### 什么是token?
这个跟路由转发其实没什么关系,token是属于socket stream的,类似于文件描述符的概念,与stream对象一一对应.

### 什么是service\_id ?
它是服务进程在集群中的唯一标识,一般可以在进程的命令行参数(或配置文件)中指定.
它是一个32位整数,其高16位表示服务类型(如,邮件服务,聊天服务等等),服务类型的取值范围为[0, 255],而低16位表示进程的实例编号.
比如我们把邮件服务定义为2,并同时运行了3个邮件服务实例,那么他们的服务ID就分别是: 2.1, 2.2, 2.3,类似于IP地址.

### 怎么理解这里的'高可用'?
这里主要指服务不因单个进程失效而不可用;单个进程失效时,负载自动被切换到同一功能组的其他进程.
根据服务进程的业务特点,可以把它们按照主从,哈希,随机,广播等方式组织.
故障切换是用来提高服务的可用性,并非说切换是无损的,在切换期间,势必会有一些业务消息丢失.

### router如果运行多个实例,如何保障他们的一致性?
这里的一致性,主要涉及哈希,随机,主从三个方式.
其中哈希和随机能够多机一致,是因为路由模块内部对service\_id做了排序.
当然,是否存在某些极其特殊的场景,使得多个router之间的哈希数组不一致呢?
这显然是可以构造出来的,所以说,这里的'高可用'只是提高可用性,而不是绝对保障.
至于master/slave的仲裁,这里推荐的方式是采用租约的方式,并将租约写入数据库.
所有进程都不断尝试取得租约(或续约),比如每秒一次,租约一旦超时(比如10秒),那么其他进程就能够取得租约,从而切换为master.
发生主从切换后,这个主从信息会同步到具体的服务进程,以及所有的router.

### 如果运行多个router,那么服务进程如何选择router?
服务进程同时连接所有的router,但只会随机的选择一个router来做消息转发的.
一般可以在router连接成功/连接丢失时,随机选择一个router.
切勿在每次发送消息的时候进行随机,这样可能会带来消息之间的时序问题.

### 如何控制路由转发?

路由转发是按功能组控制的,每组路由主要包含两个信息:
1. 实例数组(即连接的token数组),它用于做哈希转发或随机转发.
2. master,即主进程的连接token.
通过socket_mgr.register接口可以控制内部的路由表.

```lua
--id,服务进程的id,即service_id,32位整数
--token,连接的唯一标识(stream.token),传入0表示保留空位,传入nil表示从路由表删除
--is_master,是否master
socket_mgr.register(id, token, is_master);
```

### 加入了高可用支持,会不会使得业务编码很麻烦?

'高可用'当然离不开业务逻辑设计上的支持.
比如邮件服务,实现邮件服务的开发者当然应该它是按哈希分布,还是按主从分布,如果是哈希分布,那用什么做KEY.
而调用邮件服务的人,当然也应该知道邮件服务是怎么分布的,如果是哈希分布,还得提供KEY.
但这些都是可以简单封装的,比如我们把邮件服务按照玩家账号(acc)来做哈希分布,那么可以这样封装:

```lua
--my_router: 当前首选router
--mailsvr_group,服务分组编号,预定义的常量
function call_mailsvr(key, msg, ...)
	my_router.forward_hash(key, mailsvr_group, msg, ...);
end

--业务调用:
call_mailsvr(player.acc, "send_mail", player.acc, receiver, "hello", "balabala...");
```




