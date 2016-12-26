#ifndef __WATCHSERVER_H__
#define __WATCHSERVER_H__
#include "epollevent.h"
#include "filter.h"
#include "qfifo.h"
#include "dllist.h"


typedef struct logObj
{
    int pri;
    char* hostName;
    char* logLine;
    int lineLen;
    
} logObj_t;


typedef struct watcher {
    DL_NODE node;
    //int sock;
    eventConn_t* ec;
    tfilter_t* filter;
} watcher_t;


typedef struct watchProto {
    char pattern[256];
} watchProto_t;


int startWatchServer(qfifo_t* qfifo);

#endif

