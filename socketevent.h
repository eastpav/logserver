#ifndef __SOCKETEVENT_H__
#define __SOCKETEVENT_H__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int creatTcpServer(int port);
int socketAccept(int sock, struct sockaddr* addr, socklen_t* len);
int socketListen(int sock, int max);
int creatTcpClient(void);
int socketConnect(int sock, char* host, int port);
int sendData(int sock, char* buffer, int length, int* sentLen);
int recvData(int sock, char* buffer, int length, int* receivedLen);

#endif

