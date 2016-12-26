#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "metatype.h"
#include "socketevent.h"
#include "logger.h"

#define VERSION "1.0"
#define PATCHLEVEL "00"

char* g_confFile = NULL;
char* g_hostAddr = "192.168.0.102";
char* g_patternExp = "all"; /* receive all log line, by default */
int g_hostPort = 1234;
int g_debugOn = 0;


int usage(void)
{
	fprintf(stderr, "usage: logwatch [-dv] [-e expression] [-f conffile] [-h host] [-p port]\n");
    fprintf(stderr, "        %-*s %s\n", 25, "-d", "open the debug flag");
    fprintf(stderr, "        %-*s %s\n", 25, "-e expression", "set filter string(default:all)");
    fprintf(stderr, "        %-*s %s\n", 25, "-f conffile", "indicate configuration file");
    fprintf(stderr, "        %-*s %s\n", 25, "-h host", "indicate host name or address(default:localhost)");
    fprintf(stderr, "        %-*s %s\n", 25, "-p port", "indicate host port(default:1234)");
	exit(1);
}

int main(int argc, char** argv)
{
    int ch;
    extern int optind;
	extern char *optarg;

    while ((ch = getopt(argc, argv, "de:f:h:p:v")) != EOF) {
		
        switch((char)ch) 
        {
		case 'd':		/* debug */
			g_debugOn = 1;
			break;
        case 'e':		/* filter expression string */
			g_patternExp = optarg;
            break;
		case 'f':		/* configuration file */
			g_confFile = optarg;
            break;
        case 'h':		/* host address */
			g_hostAddr = optarg;
			break;
        case 'p':       /* host port */
            g_hostPort = atoi(optarg);
            break;
		case 'v':
			printf("logwatch %s.%s\n", VERSION, PATCHLEVEL);
			exit (0);
		case '?':
		default:
			usage();
		}
    }

    if((argc -= optind))
		usage();

    int sock;
    sock = creatTcpClient();
    if(sock < 0) {
        printf("socket create failed\n");
        return -1;
    }

    if(socketConnect(sock, g_hostAddr, g_hostPort) != OP_OK) {
        printf("socket connect to %s:%d failed\n", g_hostAddr, g_hostPort);
        close(sock);
        return -1;
    }

    protoHead_t protoHead;
    commuHead_t commuHead;

    int sentLen = 0;

    commuHead.magic = COMMU_HEAD_MAGIC;
    commuHead.type = COMMU_TYPE_REQUEST;
    strcpy(commuHead.dataStr, g_patternExp);
    
    protoHead.magic = PROTO_HEAD_MAGIC;
    protoHead.dataLen = sizeof(commuHead_t);

    sendData(sock, (char*)&protoHead, sizeof(protoHead), &sentLen);
    sendData(sock, (char*)&commuHead, sizeof(commuHead_t), &sentLen);

    commuHead_t recvdHead;
    int recvLen = 0;
    recvData(sock, (char*)&protoHead, sizeof(protoHead), &recvLen);
    if(recvLen != sizeof(protoHead)) {
        printf("(%d)received packet with error bytes(%ld->%d)\n", __LINE__, sizeof(protoHead), recvLen);
        close(sock);
        return -1;
    }

    recvData(sock, (char*)&recvdHead, sizeof(commuHead_t), &recvLen);
    if(recvdHead.magic != COMMU_HEAD_MAGIC) {
        printf("received packet with error magic(%d)\n", recvdHead.magic);
        close(sock);
        return -1;
    }

    if(recvdHead.type != COMMU_TYPE_ANSWER) {
        printf("received packet with error type(%d)\n", recvdHead.type);
        close(sock);
        return -1;
    }

    if(recvdHead.follow != OP_OK) {
        printf("failed, because of %s\n", recvdHead.dataStr);
        close(sock);
        return -1;
    }

    char buffer[MAX_DATE_STR_LEN];
    int logLen = 0;
    /* all is ok, ready to receive my logs */
    logInfo("all is ok\n");
    while(1) {
        logLen = recv(sock, buffer, MAX_DATE_STR_LEN, 0);
        buffer[logLen] = 0;
        printf("%s\n", buffer);
        
    }
    
    return 0;
}
