#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "proto.h"
#include "metatype.h"
#include "logger.h"

static GHashTable *g_protoNameMap = NULL;
static char *g_lastFieldName = NULL;
static GPtrArray *g_protoArray;

gboolean protoInit(void)
{
    g_protoNameMap = g_hash_table_new(g_str_hash, g_str_equal);
    g_protoArray = g_ptr_array_new();
    
    return TRUE;
}

gboolean protoRegister(char *protoName, char *type)
{
    sttype_id_t pv;

    if(g_strcmp0(type, "string") == 0)
        pv = STTYPE_STRING;
    else if(g_strcmp0(type, "integer") == 0)
        pv = STTYPE_INTEGER;
    else
        return FALSE;

    /* 用于保存proto的顺序，hash表是按字母排序，会打乱proto的顺序，在logline解析时，需要保持顺序 */
    g_ptr_array_add(g_protoArray, (gpointer)g_strdup(protoName));
    
    return g_hash_table_insert(g_protoNameMap, g_strdup(protoName), (gpointer)pv);
}

gboolean protoExist(const char* fieldName)
{
    gboolean existed = FALSE;
    if(!fieldName) return FALSE;
    
    if (g_strcmp0(fieldName, g_lastFieldName) == 0) 
    {
		return TRUE;
	}

    existed = g_hash_table_contains(g_protoNameMap, fieldName);
    if(existed)
    {
        g_free(g_lastFieldName);
        g_lastFieldName = g_strdup(fieldName);
        return TRUE;
    }

    return FALSE;
}

gboolean protoTypeGetByName(const char* fieldName, sttype_id_t *protoValue)
{
    gpointer v = NULL;
    
    if(!fieldName) return FALSE;

    /* 使用g_hash_table_lookup函数时，要求key对应的value值必须大于0，否则返回0不能区分是失败还是数据 */
    v = g_hash_table_lookup(g_protoNameMap, fieldName);
    if(v)
    {
        *protoValue = (sttype_id_t)v;
        return TRUE;
    }

    return FALSE;
}

gboolean protoExplore(char* confPath)
{
    /* every line has this proto */
    protoRegister("pri", "integer");
    protoRegister("from", "string");
    
    FILE* fp = fopen(confPath, "r");
    if(fp)
    {
        char* line = NULL;
        char* striped = NULL;
        size_t length = 0;
        int read = 0;

        char** section = NULL;
        char** proto = NULL;

        while ((read = getline(&line, &length, fp)) != -1) {
            //printf("(%d:%ld)%s\n", read, strlen(line), line);
            section = g_strsplit(line, "#", 2);
            striped = g_strstrip(*section);

            if(strlen(striped) > 0)
            {
                proto = g_strsplit(striped, " ", 2);
                g_assert(*proto != 0 && *(proto+1) != 0);
                protoRegister(g_strstrip(*proto), g_strstrip(*(proto+1)));
                g_strfreev(proto);
                proto = NULL;
            }

            g_strfreev(section);
            section = NULL;
        }

        free(line);
        fclose(fp);
        return TRUE;
    }

    return FALSE;
}

protoNode_t* newStrNode(char* key, char* value)
{
    protoNode_t* node = NULL;
    sttype_id_t protoType;

    if(protoTypeGetByName(key, &protoType) !=TRUE) {
        logWarn("Not supported proto:%s\n", key);
        return NULL;
    }
    
    if(protoType != STTYPE_STRING) {
        logWarn("Proto:%s expect integer, but encountered string\n", key);
        return NULL;
    }

    node = (protoNode_t*)malloc(sizeof(protoNode_t));
    if(node == NULL) {
        logErr("Not enough memory\n");
        return NULL; 
    }

    node->protoName = g_strdup(key);
    node->protoType = STTYPE_STRING;
    node->data = g_strdup(value);

    /* no use, just set to 0 */
    node->value = 0;

    return node;
}

int setStrNode(protoNode_t* node, char* value)
{
    if(node->protoType != STTYPE_STRING) {
        logWarn("Proto:%s expect integer, but encountered string\n", node->protoName);
        return FALSE;
    }

    if(node->data) g_free(node->data);
    node->data = g_strdup(value);
    
    return TRUE;
}

protoNode_t* newIntNode(char* key, int value)
{
    protoNode_t* node = NULL;
    sttype_id_t protoType;

    if(protoTypeGetByName(key, &protoType) !=TRUE) {
        logWarn("Not supported proto:%s\n", key);
        return NULL;
    }
    
    if(protoType != STTYPE_INTEGER) {
        logWarn("Proto:%s expect string, but encountered integer\n", key);
        return NULL;
    }

    node = (protoNode_t*)malloc(sizeof(protoNode_t));
    if(node == NULL) {
        logErr("Not enough memory\n");
        return NULL; 
    }

    node->protoName = g_strdup(key);
    node->protoType = STTYPE_INTEGER;
    node->value = value;

    /* no use, just set to 0 */
    node->data = NULL;

    return node;
}

int setIntNode(protoNode_t* node, int value)
{
    if(node->protoType != STTYPE_INTEGER) {
        logWarn("Proto:%s expect string, but encountered integer\n", node->protoName);
        return FALSE;
    }

    node->value = value;
    
    return TRUE;
}

void nodeFree(gpointer data)
{
    protoNode_t* node = (protoNode_t*)data;
    if(node->protoName) g_free(node->protoName);
    if(node->data) g_free(node->data);

    free(node);
}

int parseString(GHashTable *parseTable, int pri, char* from, char* string)
{
    protoNode_t* node = NULL;
    char** section = NULL;
    char** tmp = NULL;
    char* protoStr = NULL;
    int i = 0;
    char *p = string;
    char *s = NULL;

    /*
     * 寻找proto字符串，处于[]之间，并用" "间隔的字符串
     */
    if(*p != '[') {
        logWarn("log line with no proto, ignored\n");
        return OP_ERROR;
    }
    
    p++;
    s = p;
    while(*p != ']') {
        i++;
        p++;
    }

    protoStr = (char*)malloc(i+1);
    if(protoStr == NULL) {
        logWarn("Not enough memory!\n");
        return OP_ERROR;
    }

    memcpy(protoStr, s, i);
    protoStr[i] = '\0';

    /* 将pri、from和proto字符串依次组合成数组，顺序需要保持 */
    GPtrArray *array;
    char intStr[64];
    array = g_ptr_array_new();
    sprintf(intStr, "%d", pri);
    g_ptr_array_add(array, (gpointer)g_strdup(intStr));
    g_ptr_array_add(array, (gpointer)g_strdup(from));
        
    section = g_strsplit(protoStr, " ", -1);
    tmp = section;
    while(*tmp != NULL) {
        g_ptr_array_add(array, (gpointer)g_strdup(*tmp));
        tmp++;
    }
    g_strfreev(section);
    free(protoStr);

    int idx = 0;
    char* keyvalue = NULL;
    char* key;
    sttype_id_t value;
    gpointer v = NULL;

    /* 
     * 每次被扫描的logObj包含相同的key，所以后续直接使用第一次创建的节点。
     * 此处默认log line的proto于配置的顺序一致，需人为保证。
     */
    
    //<0>[file line devId type port]information
    while ((keyvalue = g_ptr_array_index(array, idx)) != NULL 
            && (key = g_ptr_array_index(g_protoArray, idx)) != NULL) {
        // do something with key and value
        v = g_hash_table_lookup(g_protoNameMap, key);
        if(v == NULL) {
            logErr("Unknown proto(%s) in log line\n", key);
            g_ptr_array_free(array, TRUE);
            return OP_ERROR;
        }
        value = (sttype_id_t)v;

        if(value == STTYPE_STRING) {
            node = newStrNode(key, keyvalue);
        } else {
            node = newIntNode(key, atoi(keyvalue));
        }
        idx++;
        g_hash_table_insert(parseTable, key, (gpointer)node);
    } 

    g_ptr_array_free(array, TRUE);

    return OP_OK;
}


