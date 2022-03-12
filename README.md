# 异步日志
想要实现异步，只需要加中间件即可。加入队列。
这里采用双队列。一个队列用于存放空闲buffer，另一个队列用于存放需要写入的文件的buffer，
## Simple Performance Test
单线程：1百万条log写入，耗时471ms，212w logs/second  
多线程：4线程各1百万条log写入，耗时2635ms，151w logs/second