# RPC

多个客户端同时调用存储在服务器上函数库里的函数，服务器返回函数运行的结果，且可以使用 cmd 管理服务器。项目主要用 C++ 开发，使用 Asio 网络库来满足项目源码跨平台的需求，端与端间采用 JSON 格式传输数据。可以对网络相关类型进行 mock，无网络通信下测试服务器代码。

C++ 开发内容包括：一个经过简化的、类似 Catch2 用法的测试框架，B+ 树的模板库，JSON 序列化和反序列化库，数据持久化库，文件中对象内的数据惰性读取以及一系列类似将硬盘文件当作内存使用的功能（用途之一：将 B+ 树持久化到硬盘上作为函数库的索引文件），包含协程的服务器应答过程等。

此外还开发了 web 前端网页和 C# 程序来生成部分测试代码，辅助验证程序正确性。

[项目代码及详细说明](../../tree/master/src)