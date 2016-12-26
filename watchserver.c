#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "metatype.h"
#include "logger.h"
#include "proto.h"
#include "socketevent.h"
#include "epollevent.h"
#include "watchserver.h"


DL_LIST g_watcherList;

static char tmpBuffer[MAX_REMAINING_BUF_LEN];

int addWatchList(struct eventConn* nConn, tfilter_t* filter)
{
    watcher_t* aWatch = (watcher_t*)malloc(sizeof(watcher_t));
    if(aWatch == NULL) {
        logWarn("Not enough memory!\n");
        return OP_ERROR;
    }

    //aWatch->sock = nConn->fd;
    aWatch->ec = nConn;
    aWatch->filter = filter;

    dllAdd(&g_watcherList, (DL_NODE*)aWatch);

    nConn->pointer = (void*)aWatch;

    return OP_OK;
}

int removeClient(eventHandler_t* handler, struct eventConn* nConn)
{
    watcher_t* aWatch = (watcher_t*)nConn->pointer;
    //epollDelEvent(handler, nConn);    
    close(nConn->fd);
    logInfo("close:%d\n", nConn->fd);
    if(aWatch != NULL) {
        cleanFilter(aWatch->filter);
        dllRemove(&g_watcherList, (DL_NODE*)nConn->pointer);
        free(aWatch);
    }
    free(nConn);

    return OP_OK;
}

int sendPacket(struct eventConn* nConn, char* buffer, int packetLen);

int processRequest(struct eventConn* nConn, char* data, int dataLen)
{
    tfilter_t* filter = NULL;
    commuHead_t* watch = NULL;
    commuHead_t answer;

    if(dataLen != sizeof(commuHead_t)) {
        logWarn("Request invalid!\n");
        return OP_ERROR;
    }

    watch = (commuHead_t*)data;
    if(watch->type != COMMU_TYPE_REQUEST) {
        logWarn("Invalid request type(%d)\n", watch->type);
        answer.magic = COMMU_HEAD_MAGIC;
        answer.type = COMMU_TYPE_ANSWER;
        answer.follow = OP_ERROR;
        sprintf(answer.dataStr, "Invalid request type");
        sendPacket(nConn, (char*)&answer, sizeof(commuHead_t));
        return OP_ERROR;
    }

    logInfo("filter string:%s\n", watch->dataStr);
    filter = compile(watch->dataStr);
    if(filter == NULL) {
        logWarn("Invalid filter string!\n");
        answer.magic = COMMU_HEAD_MAGIC;
        answer.type = COMMU_TYPE_ANSWER;
        answer.follow = OP_ERROR;
        sprintf(answer.dataStr, "Invalid filter string");
        sendPacket(nConn, (char*)&answer, sizeof(commuHead_t));
        return OP_ERROR;
    }
    
    answer.magic = COMMU_HEAD_MAGIC;
    answer.type = COMMU_TYPE_ANSWER;
    answer.follow = OP_OK;

    if(sendPacket(nConn, (char*)&answer, sizeof(commuHead_t)) != OP_OK) {
        logErr("send failed!\n");
        cleanFilter(filter);
        return OP_ERROR;
    }

    return addWatchList(nConn, filter);
}

int sendPacket(struct eventConn* nConn, char* buffer, int packetLen)
{
    protoHead_t head;
    head.magic = PROTO_HEAD_MAGIC;
    head.dataLen = packetLen;
    
    if(send(nConn->fd, (char*)&head, sizeof(protoHead_t), 0) == -1) {
        return OP_ERROR;
    }

    if(send(nConn->fd, buffer, packetLen, 0) == -1) {
        return OP_ERROR;
    }

    return OP_OK;
}

int recvPacket(struct eventConn* nConn, char* buffer, int* readLen)
{
    protoHead_t head;
    int recvLen = 0;
    int ret = OP_OK;
    char* p = NULL;

    if(nConn->requireLen != 0) { //缓冲区中还有不完整的数据
        p = nConn->remaining + nConn->remainingLen;
        ret = recvData(nConn->fd, p, nConn->requireLen, &recvLen);
        if(ret != OP_OK) {
            if(ret != OP_EAGAIN) {
                return OP_ERROR;
            } else {
                nConn->requireLen = nConn->requireLen - recvLen;
                nConn->remainingLen = nConn->remainingLen + recvLen;
                return OP_EAGAIN;
            }
        }

        nConn->requireLen = 0;

        if(nConn->remainIsHead) {
            memcpy(&head, nConn->remaining, sizeof(protoHead_t));
            nConn->remainIsHead = FALSE;
            ret = recvData(nConn->fd, nConn->remaining, head.dataLen, &recvLen);
            if(ret != OP_OK) {
                if(ret != OP_EAGAIN) {
                    return OP_ERROR;
                } else {
                    nConn->requireLen = head.dataLen - recvLen;
                    nConn->remainingLen = recvLen;
                    return OP_EAGAIN;
                }
            }

            memcpy(buffer, nConn->remaining, recvLen);
            *readLen = recvLen;
            return OP_OK;
        } else {
            memcpy(buffer, nConn->remaining, nConn->remainingLen + recvLen);
            *readLen = nConn->remainingLen + recvLen;
            return OP_OK;
        }
    } else {
        
        if(!nConn->remainIsHead) {
            nConn->remainIsHead = TRUE;
            ret = recvData(nConn->fd, nConn->remaining, sizeof(protoHead_t), &recvLen);
            if(ret != OP_OK) {
                if(ret != OP_EAGAIN) {
                    return OP_ERROR;
                } else {
                    nConn->requireLen = sizeof(protoHead_t) - recvLen;
                    nConn->remainingLen = recvLen;
                    return OP_EAGAIN;
                }
            }

            memcpy(&head, nConn->remaining, sizeof(protoHead_t));
            nConn->remainIsHead = FALSE;
            ret = recvData(nConn->fd, nConn->remaining, head.dataLen, &recvLen);
            if(ret != OP_OK) {
                if(ret != OP_EAGAIN) {
                    return OP_ERROR;
                } else {
                    nConn->requireLen = head.dataLen - recvLen;
                    nConn->remainingLen = recvLen;
                    return OP_EAGAIN;
                }
            }

            memcpy(buffer, nConn->remaining, recvLen);
            *readLen = recvLen;
            return OP_OK;
        }

    }

    return OP_ERROR;
}

int dataHandler(eventHandler_t* handler, struct eventConn* nConn, int revents)
{
    int ret = 0;
    int readLen = 0;

    if (revents & EPOLLERR || revents & EPOLLRDHUP) {
        logDebug("dataHandler() error on fd:%d ev:%04X\n", nConn->fd, revents);
        removeClient(handler, nConn);
        return OP_OK;
    }

    /* 默认使用epoll ET模式，所以需要一次性处理完当前该连接所有的数据 */
    while(1) {
        ret = recvPacket(nConn, tmpBuffer, &readLen);
        if(ret != OP_OK) {
            if(ret != OP_EAGAIN) {
                /* 客户端关闭 */
                removeClient(handler, nConn);
                break;
            } else {
                /* 数据包未接收完整 */
                break;
            }
        }

        if(processRequest(nConn, tmpBuffer, readLen) != OP_OK) {
            removeClient(handler, nConn);
            break;
        }
    }

    return OP_OK;
}

int connectionHandler(eventHandler_t* handler, struct eventConn* aConn, int revents)
{
    struct sockaddr_in in_addr;
    socklen_t in_len = sizeof(struct sockaddr);
    int infd;
    eventConn_t* nConn = NULL;

    /* 默认使用epoll ET模式，所以需要一次性处理完当前该连接所有的事件 */
    while(1) {
        infd = socketAccept(aConn->fd, (struct sockaddr*)&in_addr, &in_len);
        if(infd == -1) {
            if(errno == EAGAIN
                || errno == EWOULDBLOCK) {
                break;
            } else {
                //logErr("accept socket failed with (%d)\n", aConn->fd);
                perror("accept");
                break;
            }
        }

        if(setNonblocking(infd) < 0) {
            close(infd);
            break;
        }

        nConn = (eventConn_t*)malloc(sizeof(eventConn_t));
        if(nConn == NULL) {
            close(infd);
            break;
        }

        memset(nConn, 0, sizeof(eventConn_t));
        logInfo("New connection:%d from:%s\n", infd, inet_ntoa(in_addr.sin_addr));
        nConn->fd = infd;
        nConn->handleData = dataHandler;
        nConn->pointer = NULL;

        if(epollAddEvent(handler, nConn) < 0) {
            close(infd);
            free(nConn);
            continue;
        }
    }

    return OP_OK;
}

void* watchConnServer(void* arg)
{
    eventConn_t* econn;
    eventHandler_t *handler = NULL;
    int sock;

    handler = epollInit(512);
    assert(handler != NULL);

    sock = creatTcpServer(1234);
    if(sock < 0) {
        goto failure1;
    }

    if(setNonblocking(sock) < 0) {
        goto failure2;
    }

    if(socketListen(sock, 100) < 0) {
        logErr("listen socket failed with (%d)\n", sock); 
        goto failure2;
    }

    econn = (eventConn_t*)malloc(sizeof(eventConn_t));
    if(econn == NULL) {
        logErr("alloc memory failed\n"); 
        goto failure2;
    }

    econn->fd = sock;
    econn->handleData = connectionHandler;

    dllInit(&g_watcherList);

    if(epollAddEvent(handler, econn) < 0) {
        goto failure2;
    }

    while(1) {
        /* 连接的事件到来会自动调用其数据处理回调函数 */
        epollProcessEvent(handler, 2000);
        
    }

failure2:
    close(sock);
    
failure1:
    epollClean(handler);

    return NULL;
}


int parseLog(logObj_t* logObj, GHashTable *parseTable)
{
    return parseString(parseTable, logObj->pri, logObj->hostName, logObj->logLine);
}

void filterDeliver(GHashTable *parseTable, watcher_t* aWatch, logObj_t* logObj)
{
    int sent = 0;
    int sock = aWatch->ec->fd;
    if(doFilter(aWatch->filter, parseTable) == TRUE) {
        sent = send(sock, logObj->logLine, strlen(logObj->logLine), 0);
        if(sent == -1) {
            //perror("send");
            removeClient(NULL, aWatch->ec);
        }
    }
}

void deliver(logObj_t* logObj, GHashTable *parseTable)
{
    DL_NODE *pNode, *pNext;
    pNode = DLL_FIRST(&g_watcherList);

    while(pNode != NULL) {
        pNext = DLL_NEXT (pNode);
        filterDeliver(parseTable, (watcher_t*)pNode, logObj);
        pNode = pNext;
    }
}

extern void nodeFree(gpointer p);

void printHash(GHashTable* parsedMap)
{
    GHashTableIter iter;
    gpointer key;
    gpointer value;
    protoNode_t* node = NULL;
    g_hash_table_iter_init (&iter, parsedMap);
    while (g_hash_table_iter_next (&iter, &key, &value)) {
        // do something with key and value
        node = (protoNode_t*)value;
        if(node->protoType == STTYPE_STRING)
            printf("====%s -> %s\n", node->protoName, node->data);
        else
            printf("===+%s -> %d\n", node->protoName, node->value);
    }
    
}

void* watchDispatchServer(void* arg)
{
    qfifo_t* qfifo = (qfifo_t*)arg;
    logObj_t* logObj = NULL;

    //每条日志信息按protocol解析后的Key-value存入HASH表
    GHashTable *parsedMap = NULL;
    parsedMap = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, nodeFree);
    if(parsedMap == NULL) {
        logErr("hash table create failed(%s:%d)\n", __FILE__, __LINE__);
        return NULL;
    }

    while(1)
    {
        qfifoGet(qfifo, (char*)&logObj, 4);
        if(logObj)
        {
            printf("------>%s\n", logObj->logLine);
            
            if(parseLog(logObj, parsedMap) == OP_OK) {
                //printHash(parsedMap);                
                deliver(logObj, parsedMap);
            }
            
            if(logObj->logLine) free(logObj->logLine);
            if(logObj->hostName) free(logObj->hostName);
            free(logObj);

        }
    }
    
    return NULL;
}

int startWatchServer(qfifo_t* qfifo)
{
    pthread_t connPthread;
    pthread_t dispatchPthread;

    (void) signal(SIGPIPE, SIG_IGN);

    if(pthread_create(&connPthread, NULL, watchConnServer, NULL) != 0)
    {
        return -1;
    }
    if(pthread_create(&dispatchPthread, NULL, watchDispatchServer, (void*)qfifo) != 0)
    {
        pthread_cancel(connPthread);
        return -1;
    }

    return 0;
}
