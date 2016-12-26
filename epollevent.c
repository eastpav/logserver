#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/epoll.h>
#include "epollevent.h"
#include "metatype.h"
#include "logger.h"

int defaultProcess(eventHandler_t* handler, struct eventConn* eConn, int revents);

eventHandler_t* epollInit(int maxfds)
{
    int epfd;
    eventHandler_t* ehandler = NULL;

    /* Initialize the kernel queue.  (The size field is ignored since
	 * 2.6.8.) */
	if ((epfd = epoll_create(maxfds)) == -1) {
		perror("epoll_create");
		return NULL;
	}
    
    if (!(ehandler = (eventHandler_t*)malloc(sizeof(eventHandler_t)))) {
		close(epfd);
		return NULL;
	}

    ehandler->efd = epfd;
    ehandler->events = (struct epoll_event*)malloc(INITIAL_NEVENT*sizeof(struct epoll_event));
	if (ehandler->events == NULL) {
		free(ehandler);
		close(epfd);
		return NULL;
	}
    ehandler->nevents = INITIAL_NEVENT;

    return ehandler;
}

int epollAddEvent(eventHandler_t* handler, eventConn_t* econn)
{
    if(handler == NULL) {
        logErr("epollAddSocket failed with NULL handler\n");
        return OP_ERROR;
    }

    if(econn == NULL) {
        logErr("epollAddSocket failed with NULL connection\n");
        return OP_ERROR;
    }

    struct epoll_event  ee;
    ee.events = EPOLLIN|EPOLLET|EPOLLRDHUP;
    ee.data.ptr = (void*)econn;

    econn->eventMask = ee.events;

    if (epoll_ctl(handler->efd, EPOLL_CTL_ADD, econn->fd, &ee) == -1) {
        logErr("epoll_ctl(EPOLL_CTL_ADD, %d) failed\n", handler->efd);
        return OP_ERROR;
    }

    return OP_OK;
}

int epollDelEvent(eventHandler_t* handler, eventConn_t* econn)
{
    if(handler == NULL) {
        logErr("epollDelEvent failed with NULL handler\n");
        return OP_ERROR;
    }

    if(econn == NULL) {
        logErr("epollDelEvent failed with NULL connection\n");
        return OP_ERROR;
    }

    struct epoll_event  ee;
    ee.events = 0;
    ee.data.ptr = NULL;

    if (epoll_ctl(handler->efd, EPOLL_CTL_DEL, econn->fd, &ee) == -1) {
        logErr("epoll_ctl(EPOLL_CTL_DEL, %d) failed\n", handler->efd);
        return OP_ERROR;
    }

    return OP_OK;
}

int epollProcessEvent(eventHandler_t* handler, int timeout)
{
    int rdyEvents;
    int i;
    eventConn_t* eConn = NULL;
    uint32_t revents;
    struct epoll_event *events = handler->events;

    rdyEvents = epoll_wait(handler->efd, events, handler->nevents, timeout);
    if (rdyEvents == -1) {
		if (errno != EINTR) {
			logWarn("epoll_wait\n");
			return OP_ERROR;
		}

		return OP_OK;
	}

    assert(rdyEvents <= handler->nevents);
    
    for (i = 0; i < rdyEvents; i++) {
		eConn = (eventConn_t*)events[i].data.ptr;
        revents = events[i].events;

        if (revents & (EPOLLERR|EPOLLHUP)) {
            logWarn("epoll_wait() error on fd:%d ev:%04X\n", eConn->fd, revents);
        }
        
        if(eConn->handleData) {
            eConn->handleData(handler, eConn, revents);
        } else {
            defaultProcess(handler, eConn, revents);
        }
	}

	if (rdyEvents == handler->nevents && handler->nevents < MAX_EVENTS) {
		/* We used all of the event space this time.  We should
		   be ready for more events next time. */
		int new_nevents = handler->nevents * 2;
		struct epoll_event *new_events;

		new_events = (struct epoll_event*)realloc(handler->events, new_nevents * sizeof(struct epoll_event));
		if (new_events) {
			handler->events = new_events;
			handler->nevents = new_nevents;
		}
	}

	return OP_OK;
}

int defaultProcess(eventHandler_t* handler, struct eventConn* eConn, int revents)
{
    int ret = OP_OK;
    printf("defaultProcess:(%d)\n", eConn->fd);
    /* 用户未指定数据处理的方式，那么直接关闭 */
    ret = epollDelEvent(handler, eConn);
    close(eConn->fd);
    free(eConn);

    return ret;
}

int epollClean(eventHandler_t* handler)
{
    int ret = OP_OK;

    if(handler == NULL) {
        logErr("epollClean failed with NULL handler\n");
        return OP_ERROR;
    }

    if (close(handler->efd) == -1) {
        logErr("epoll close() failed\n");
        ret = OP_ERROR;
    }

    if(handler->events) {
        free(handler->events);
        handler->events = NULL;
        handler->nevents = 0;
    }

    free(handler);

    return ret;
}

/* 一些非阻塞IO需要用到的用具 */
int setNonblocking(int fd)
{
    int flags, s;

    flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) {
        perror("fcntl");
        return OP_ERROR;
    }

    flags |= O_NONBLOCK;
    s = fcntl(fd, F_SETFL, flags);
    if(s == -1) {
        perror("fcntl");
        return OP_ERROR;
    }

    return OP_OK;
}


