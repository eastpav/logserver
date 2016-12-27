#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "metatype.h"
#include "logger.h"

int creatTcpServer(int port)
{
    int sock;
    int on = 1;
    struct sockaddr_in saddr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        logErr("socket create failed\n"); 
        return OP_ERROR;
    }

    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) < 0 ) {
		logErr("setsockopt(REUSEADDR), suspending\n");
		close(OP_ERROR);
		return -1;
	}

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(port);

    if(bind(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr)) == -1) {
        logErr("bind socket failed with (%x:%d)\n", INADDR_ANY, port); 
        close(sock);
        return OP_ERROR;
    }

    return sock;
}

int socketAccept(int sock, struct sockaddr* addr, socklen_t* len)
{
    return accept(sock, addr, len);
}

int socketListen(int sock, int max)
{
    return listen(sock, max);
}

int creatTcpClient(void)
{
    int sock;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1) {
        logErr("socket create failed\n"); 
        return OP_ERROR;
    }

    return sock;
}

int socketConnect(int sock, char* host, int port)
{
    struct hostent* hostentp = NULL;
    struct sockaddr_in saddr;
    char str[32];

    if(host == NULL) return OP_ERROR;

    hostentp = gethostbyname(host);
    if(hostentp == NULL) return OP_ERROR;
    
    inet_ntop(hostentp->h_addrtype, hostentp->h_addr, str, sizeof(str));

    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = inet_addr(str);
    saddr.sin_port = htons(port);

    if(connect(sock, (struct sockaddr*)&saddr, sizeof(struct sockaddr)) != 0) {
        return OP_ERROR;
    }

    return OP_OK;
}

int sendData(int sock, char* buffer, int length, int* sentLen)
{
    int sent;
    int offset = 0;
    int len = length;
    char* buf = buffer;
    int ret = OP_OK;

    while(len) {
        sent = send(sock, buf+offset, len, 0);
        if(sent == -1) {
            if(errno == EAGAIN
                || errno == EWOULDBLOCK) {
                *sentLen = offset;
                ret =  OP_EAGAIN;
                break;
            } else {
                *sentLen = offset;
                ret = OP_ERROR;
                break;
            }
        }

        len -= sent;
		offset += sent;
    }

    *sentLen = offset;

    return ret;
}


int recvData(int sock, char* buffer, int length, int* receivedLen)
{
    int read;
    int offset = 0;
	int len = length;
	char *buf = buffer;
    int ret = OP_OK;

    while(len) {
		read = recv(sock, buf+offset, len, 0);
		if(read == 0) {
            *receivedLen = offset;
			ret = OP_ERROR;
            break;
		} else if(read == -1) {
            if(errno == EAGAIN
                || errno == EWOULDBLOCK) {
                *receivedLen = offset;
                ret =  OP_EAGAIN;
                break;
            } else {
                *receivedLen = offset;
                ret = OP_ERROR;
                break;
            }
        }

		len -= read;
		offset += read;
	}

    *receivedLen = offset;

    return ret;
}

