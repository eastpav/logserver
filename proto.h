#ifndef __PROTO_H__
#define __PROTO_H__
#include <glib.h>
#include "dllist.h"
#include "syntax-tree.h"
/*
typedef enum {
    PROTO_VALUE_UNVALID,
	PROTO_VALUE_STRING,
	PROTO_VALUE_INTEGER
} proto_value_t;*/

typedef struct protoNode {
    char* protoName;
    //proto_value_t protoType;
    sttype_id_t protoType;
    int value;
    char* data;
} protoNode_t;

gboolean protoInit(void);
gboolean protoExist(const char* fieldName);
gboolean protoTypeGetByName(const char* fieldName, sttype_id_t *protoValue);
gboolean protoExplore(char* confPath);
int parseString(GHashTable *parseTable, int pri, char* from, char* string);

#endif

