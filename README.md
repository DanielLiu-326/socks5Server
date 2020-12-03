# socks5Server
a socks5 server based on libevent(Linux only) 
## 2.0 reformed the code
## usage
`cd src`  
`cmake`  
`make`  
after a while, you would see a file named socks5Server;  
you should create a property file before start the server. the file as follow：  
```
CLIENT_TTL = 600 
#if a client connection has no data transport for CLIENT_TTL seconds,server would close the connection 
SEND_UNIT_SIZE = 1500
#the max size data server in single send
SERVER_IP = 0.0.0.0
#the socks5 server ip 0.0.0.0 is default
PORT = 10091
#port
ALLOW_ANONYMOUSE = 1
#0 or 1 if set 1 server will not authentic username and password
THREADS = 4 
#set the worker threads (not less 1)

[accounts]
danny = passwd  
```
save it as server.config and move it to the directory which saved socks5Server  
then enter: ./socks5Server the server will automatically load the properties named server.config in current work directory you can also use the path what you want just add the path after that command like  
`./socks5Server <path>`
