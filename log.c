/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

#if defined(__APPLE__)
#include <crt_externs.h>
#define HOST_NAME_MAX 255
//#else
//#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX // located in limits.h -> bits/posix1_lim.h
#endif

#include "log.h"

#define MAX_MESSAGE_LEN 65536

// From RFC3164
static const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

typedef struct {
    char hostname[HOST_NAME_MAX];
    char progname[1024];

    int syslog_facility;
    const char *syslog_tag;

    struct addrinfo *serveraddr;
    int sockfd;

    pthread_mutex_t lock;
} SharedData;

static SharedData shared_data = { "", "" };

#ifdef NDEBUG
#define DBG(x)
#else
#define DBG(x) debugprintf x

static void debugprintf(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}
#endif

static void set_defaults(SharedData *sd) {
    char *slash_ptr = strrchr(sd->progname, '/');
    // If progname contains a slash, extract basename to use it as syslog tag
    sd->syslog_tag = slash_ptr ? slash_ptr + 1 : sd->progname;
    sd->syslog_facility = 0;
}

static void init_progname(SharedData *sd) {
#if defined(__APPLE__)
    sscanf(*_NSGetProgname(), "%1023s", sd->progname);
#elif defined(__FreeBSD__)
    sscanf(getprogname(), "%1023s", sd->progname);
#else
    FILE* cmdline = fopen("/proc/self/cmdline", "rb");
    if (cmdline) {
        fscanf(cmdline, "%1023s", sd->progname);
        fclose(cmdline);
    }
#endif
}

static void init_hostname(SharedData *sd) {
    if (gethostname(sd->hostname, sizeof(sd->hostname)) != 0) {
        perror("liblogfaf: gethostname() error");
        exit(1);
    }
    // We don't really need FQDN, so we truncate the hostname up until
    // the first dot.
    char *dot_ptr = strchr(sd->hostname, '.');
    if (dot_ptr != NULL) {
        *dot_ptr = '\0';
    }
}

static void init_connection(SharedData *sd) {
    char *server_hostname;
    server_hostname = getenv("LIBLOGFAF_SERVER");
    if (server_hostname == NULL)
        server_hostname = "localhost";

    char *server_port;
    server_port = getenv("LIBLOGFAF_PORT");
    if (server_port == NULL)
        server_port = "514";

    int gai_error;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;
    printf("%s:%s\n", server_hostname, server_port);
    gai_error = getaddrinfo(server_hostname, server_port,
                            &hints, &sd->serveraddr);
    if (gai_error != 0) {
        fprintf(stderr, "liblogfaf: getaddrinfo() failed: %s\n",
                gai_strerror(gai_error));
        exit(1);
    }

    if ((sd->sockfd = socket(sd->serveraddr->ai_family,
                             sd->serveraddr->ai_socktype,
                             sd->serveraddr->ai_protocol)) < 0) {
        perror("liblogfaf: cannot create socket");
        exit(1);
    }
}

static void logmessage(SharedData *sd, int priority, const char *message) {
    DBG(("liblogfaf: logmessage(%d, %s)\n", priority, message));
    time_t ts;
    struct tm *time_tm;
    char msg[MAX_MESSAGE_LEN];
    ts = time(NULL);
    time_tm = localtime(&ts);

    snprintf(msg, MAX_MESSAGE_LEN, "<%u>%s %2d %02d:%02d:%02d %s %s: %s",
             priority + sd->syslog_facility * 8,
             months[time_tm->tm_mon], time_tm->tm_mday,
             time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec,
             (char *)&sd->hostname, sd->syslog_tag, message);

    // We want fire-and-forget, so lack of error checking here is intentional
    sendto(sd->sockfd, msg, strlen(msg), 0,
           sd->serveraddr->ai_addr, sd->serveraddr->ai_addrlen);
}

static void logmessageEx(SharedData *sd, int priority, char* spec, const char *message) {
    DBG(("liblogfaf: logmessage(%d, %s)\n", priority, message));
    time_t ts;
    struct tm *time_tm;
    char msg[MAX_MESSAGE_LEN];
    ts = time(NULL);
    time_tm = localtime(&ts);

    snprintf(msg, MAX_MESSAGE_LEN, "<%u>%s%s %2d %02d:%02d:%02d %s %s: %s",
             priority + sd->syslog_facility * 8, spec,
             months[time_tm->tm_mon], time_tm->tm_mday,
             time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec,
             (char *)&sd->hostname, sd->syslog_tag, message);

    // We want fire-and-forget, so lack of error checking here is intentional
    sendto(sd->sockfd, msg, strlen(msg), 0,
           sd->serveraddr->ai_addr, sd->serveraddr->ai_addrlen);
}

__attribute__((constructor)) static void _liblogfaf_init(void) {
    DBG(("liblogfaf: init()\n"));
    init_progname(&shared_data);
    init_hostname(&shared_data);
    init_connection(&shared_data);
    if (pthread_mutex_init(&shared_data.lock, NULL) != 0) {
        fprintf(stderr, "liblogfaf: pthread_mutex_init() failed\n");
        exit(1);
    }
    set_defaults(&shared_data);
}

__attribute__((destructor)) static void _liblogfaf_fini(void) {
    DBG(("liblogfaf: fini()\n"));
    if (pthread_mutex_destroy(&shared_data.lock) != 0) {
        fprintf(stderr, "liblogfaf: pthread_mutex_destroy() failed\n");
        exit(1);
    }
    freeaddrinfo(shared_data.serveraddr);
}

void openlog(const char *ident, int option, int facility) {
    DBG(("liblogfaf: openlog(%s, %d, %d)\n", ident, option, facility));
    if (pthread_mutex_lock(&shared_data.lock) != 0) {
        fprintf(stderr, "liblogfaf: pthread_mutex_lock() failed\n");
        exit(1);
    }
    shared_data.syslog_facility = facility;
    if (ident)
        shared_data.syslog_tag = ident;
    if (pthread_mutex_unlock(&shared_data.lock) != 0) {
        fprintf(stderr, "liblogfaf: pthread_mutex_unlock() failed\n");
        exit(1);
    }
    // making use of the `option` parameter can be added here if you need it
}

void closelog(void) {
    DBG(("liblogfaf: closelog()\n"));
    if (pthread_mutex_lock(&shared_data.lock) != 0) {
        fprintf(stderr, "liblogfaf: pthread_mutex_lock() failed\n");
        exit(1);
    }
    set_defaults(&shared_data);
    if (pthread_mutex_unlock(&shared_data.lock) != 0) {
        fprintf(stderr, "liblogfaf: pthread_mutex_unlock() failed\n");
        exit(1);
    }
}

void __syslog_chk(int priority, int flag, const char *format, ...) {
    DBG(("liblogfaf: __syslog_chk(%d, %d, %s)\n",
         priority, flag, format));
    va_list ap;
    char str[MAX_MESSAGE_LEN];
    va_start(ap, format);
    vsnprintf(str, MAX_MESSAGE_LEN, format, ap);
    va_end(ap);
    logmessage(&shared_data, priority, str);
}

void syslog(int priority, const char *format, ...) {
    DBG(("liblogfaf: syslog(%d, %s)\n", priority, format));
    va_list ap;
    char str[MAX_MESSAGE_LEN];
    va_start(ap, format);
    vsnprintf(str, MAX_MESSAGE_LEN, format, ap);
    va_end(ap);
    logmessage(&shared_data, priority, str);
}

void syslogEx(int priority, char* spec, const char *format, ...) {
    DBG(("liblogfaf: syslog(%d, %s)\n", priority, format));
    va_list ap;
    char str[MAX_MESSAGE_LEN];
    va_start(ap, format);
    vsnprintf(str, MAX_MESSAGE_LEN, format, ap);
    va_end(ap);
    logmessageEx(&shared_data, priority, spec, str);
}

/*
 * 这只是一段示例代码，实际使用时删除该段，包含该文件，调用syslogEx输出日志。
 */
int main(void)
{
    int iter = 0;
    int idx = 0;
    printf("start\n");

    //_liblogfaf_init(); //called as constructor attribute
    while(iter++ < 100) {
        //syslog(2,"testlog:%d", iter);
        idx = iter % 3;
        if(idx == 0)
            syslogEx(2, "[FFD8 sensor 120]", "testlog:%d", iter);
        if(idx == 1)
            syslogEx(2, "[FF99 sensor 120]", "testlog:%d", iter);
        if(idx == 2)
            syslogEx(2, "[EE22 sensor 120]", "testlog:%d", iter);
        sleep(2);
    }
    printf("completed\n");
    //_liblogfaf_fini(); //called as destructor attribute

    return 0;
}

