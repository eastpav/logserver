#ifndef __METATYPE_H__
#define __METATYPE_H__

#define OP_OK       0
#define OP_ERROR   -1
#define OP_EAGAIN  -2

#define PROTO_HEAD_MAGIC 0xcaca0520

#define COMMU_HEAD_MAGIC 0x05200520
#define MAX_DATE_STR_LEN (512) //must be less than 2032, because the max buffer in server is 2048

typedef enum {
    COMMU_TYPE_UNKOWN,
    COMMU_TYPE_REQUEST,  /* must be acknowledge */
    COMMU_TYPE_ANSWER,   /* never be send proactively */
    COMMU_TYPE_MSG,      /* normal message between pair */
} commuType_t;

typedef struct protoHead {
    int magic;   /* always be 0xcaca0520 */
    int dataLen; 
} protoHead_t;

typedef struct commuHead {
    int magic;   /* always be 0x05200520 */
    commuType_t type;
    int follow;  /* only used if type is COMMU_TYPE_ANSWER, indicate success or error */

    /*
     * dataStr has two usage.
     * 1. save request pattern string if type is COMMU_TYPE_REQUEST.
     * 2. save answer message(or reason of error) if type is COMMU_TYPE_ANSWER.
     * 3. other, no use.
     */
    char dataStr[MAX_DATE_STR_LEN];
} commuHead_t;



#endif

