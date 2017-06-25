# `luabus`

luabus是一个为LUA RPC服务集群提供网络支持的基础库,主要分为网络库和路由支持两块功能.

主要特性:
- 方便的搭建任意拓扑结构集群.
- 方便的在集群进程间进行RPC调用.
- 方便的实现主要的集中消息转发方式如: 定点发送,哈希转发,随机转发,组内广播,主从备份.

## 编译环境

目前支持Windows, Linux, MacOS三平台,编译器必须支持C++14.  

- Windows: Visual studio 2015以上版本,需要自行编译lua的dll库,或者用win64目录下的库文件.
- MacOS: 需要自行编译安装lua.
- Linux: 需要自行编译安装lua.

## 网络
首先,你需要一个socket\_mgr对象:

```lua
socket_mgr = hive.create_socket_mgr(最大句柄数);
--用户需要在主循环中调用wait函数,比如:
--其参数为最大阻塞时长(实际是传给epoll_wait之类函数),单位ms.
socket_mgr.wait(50);
```

监听端口:

```lua
--注意保持listener的生存期,一旦被gc,则端口就自动关了
listener = mgr.listen("127.0.0.1", 8080);
```

发起连接:

```lua
--注意保持stream对象生存期,被gc的话,连接会自动关闭
--连接都是异步进行的,一直要stream上面触发'on_connected'事件后,连接才可用
--发生任何错误时,stream会触发'on_error'事件.
stream = mgr.connect("127.0.0.1", 8080);
```

向对端发送消息:

```lua
--TODO: 将来还可能在socket_mgr上增加广播方法
stream.call("on_login", acc, password);
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

主动断开:

```lua
--调用close即可:
stream.close();
```

## 路由转发(实现中):
作为例子,这里假定一个游戏服务集群由一下这些进程构成:
- 路由转发进程(router),集群中运行多个,但我们先考虑一个的情况. 
- 邮件服务进程(mailsvr),根据用户名哈希来分担负载
- 房间匹配服务进程(matchsvr),运行两个,主从备份.
- 游戏大厅服务器(gamnesvr),运行若干个,按在线人数负载分担.

为了实现这个路由转发,我们把所有的服务进程都连接到router,以它为中心做中转.  
另外,还需要引入服务进程标识(service id),它通常是服务进程启动时指定的(如命令行参数).
实际上它是个uint32\_t整数,最高字节[1-255]被用做服务类型(service class),其余3字节用作实例标识(instance id).  
也就是说: service\_id = (service\_class << 24) | instance\_id;  
实现中约定, service\_class, instance\_id均不为0

现在考虑router如何实现这个数据转发.  
最简单的,当然可以通过上面的远程调用来实现转发.  
但是这样有个效率问题,应该序列化数据完全没必须要在router展开,然后再重新序列化.  
作为一个业务繁忙的数据转发器,这种无谓的消耗是无法接受的.  
在数据转发逻辑中,最关键的是维护一个转发目标的集合.   
有了这个集合,我们就能实现主从,哈希,随机,指定目标等方式的转发.  
基于这个思路,我们把转发操作本身实现在C++层面,但是把集合的维护交给lua控制.  
当有服务进程连接或者断开时,可以通过下面的方式更新转发表.  

```lua
--socket_mgr内部会对同一类型的instance_id排序(升序)
--如果连接断开(或未连接)时,需要保留空位(固定哈希),可以把token传0,需要删除则设置token为nil
--目前不支持权重参数设置,可以为一个连接配置多个instance_id来实现
--主从备份模式时,永远转发给排序后的第一个非0的instance_id
socket_mgr.route(service_id, stream.token);
```

这样,比如gamesvr想把玩家加入战斗匹配,就可以额做类似这样的调用:

```lua
--注意,matchsvr的主从备份模式对调用者而言应该是透明的
--router在做消息转发的时候,会根据主从,哈希等模式参数来把消息转发到正确的目标. 
--当然,主从切换时,还是会对个别请求产生影响
socket = socket_mgr.connect("127.0.0.1", 2000);
function call_matchsvr(msg, ...)
	--将后面的参数打包给router,让它按规则(即主从备份master)转发给matchsvr
	socket.forward(master, matchsvr, msg, ...);
end

call_matchsvr("join_match", player_id, match_mode);
```




