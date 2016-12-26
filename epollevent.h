#ifndef __EPOLLEVENT_H_
#define __EPOLLEVENT_H_

#define INITIAL_NEVENT 32
#define MAX_EVENTS 4096

#define MAX_REMAINING_BUF_LEN 2048

typedef struct eventHandler
{
    int efd;
    int nevents;
    struct epoll_event* events;
} eventHandler_t;

typedef struct eventConn
{
    int fd;
    int eventMask;
    void* pointer;     /* internal use */    

    int requireLen;                        /*  */
    int remainIsHead;
    int remainingLen;
    char remaining[MAX_REMAINING_BUF_LEN];

    int (*handleData)(eventHandler_t* handler, struct eventConn* eConn, int revents);
    
} eventConn_t;

eventHandler_t* epollInit(int maxfds);
int epollProcessEvent(eventHandler_t* handler, int timeout);
int epollAddEvent(eventHandler_t* handler, eventConn_t* econn);
int epollDelEvent(eventHandler_t* handler, eventConn_t* econn);
int epollClean(eventHandler_t* handler);
int setNonblocking(int fd);

#endif

