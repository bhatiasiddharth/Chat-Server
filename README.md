# Chat-Server
Built two concurrent chat servers providing concurrency through prethreading and epoll api. No busy waiting or blocking on I/O. Clients could use JOIN, LIST, UMSG, BMSG and LEAVE commands.

## Getting started
1. Server: make
2. Client: telnet localhost 1234
