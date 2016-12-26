#include <stdio.h>
#include <stdlib.h>
#include "filter.h"
#include "grammar.h"
#include "scanner.h"
#include "sttype-test.h"
#include "proto.h"
#include "metatype.h"
#include "semcheck.h"
#include "logger.h"



void* TfilterAlloc(void* (*)(gsize));
void* Tfilter(void*, int, stnode_t*, tfilter_t*);
void* TfilterFree(void*, void(*)(void*));

static int set_lval(stnode_t* node, int token, void* data);
static int set_lval_int(tfilter_t *tf, stnode_t* node, int token, char *s);
static gboolean str_to_gint32(tfilter_t *tf, char *s, int* pint);

void tfilter_fail(tfilter_t *tf, const char *format, ...)
{
	va_list	args;

	/* If we've already reported one error, don't overwite it */
	if (tf->errorMessage != NULL)
		return;

	va_start(args, format);
	tf->errorMessage = g_strdup_vprintf(format, args);
	va_end(args);
}

static tfilter_t* tfilter_new(void)
{
	tfilter_t *tf;

	tf = g_new0(tfilter_t, 1);

	return tf;
}

static void tfilter_free(tfilter_t *tf)
{
	if (tf->stRoot) {
		stnode_free(tf->stRoot);
	}

	/*
	 * We don't free the error message string; our caller will return
	 * it to its caller.
	 */
	g_free(tf);
}

void printnode(stnode_t *node)
{
    if(node->type == STTYPE_TEST) {
        test_t	*test = (test_t*)stnode_data(node);
        printf("op:%s->\n", operandname(test->op));
        printnode(test->val1);
        printnode(test->val2);
    } else if(node->type == STTYPE_INTEGER){
        printf("%d\n", node->value);
    } else {
        printf("%s\n", (char*)node->data);
    }
}

void print(tfilter_t *tf)
{
    stnode_t *node = tf->stRoot;

    printnode(node);
}

gboolean stringTest(test_op_t op, char* val1, char* val2)
{
    gboolean ret = FALSE;
    switch(op)
    {
        case TEST_OP_EQ:
            if(strcmp(val1, val2) == 0) ret = TRUE;
        break;
        case TEST_OP_NE:
            if(strcmp(val1, val2) != 0) ret = TRUE;
        break;
        case TEST_OP_GE:
        case TEST_OP_LT:
        case TEST_OP_LE:
        default:
        break;
    }

    return ret;
}

gboolean integerTest(test_op_t op, int val1, int val2)
{
    gboolean ret = FALSE;
    switch(op)
    {
        case TEST_OP_EQ:
            if(val2 == val1) ret = TRUE;
        break;
        case TEST_OP_NE:
            if(val2 != val1) ret = TRUE;
        break;
        case TEST_OP_GT:
            if(val2 > val1) ret = TRUE;
        break;
        case TEST_OP_GE:
            if(val2 >= val1) ret = TRUE;
        break;
        case TEST_OP_LT:
            if(val2 < val1) ret = TRUE;
        break;
        case TEST_OP_LE:
            if(val2 <= val1) ret = TRUE;
        break;
        default:
        break;
    }

    return ret;
}

gboolean cmpPattern(stnode_t* keynode , stnode_t* valuenode, test_op_t op, GHashTable *parseTable)
{
    char* key = keynode->data;
    /* filter中的值 */
    int ivalue = 0;     
    char* svalue = NULL;

    /* 日志行中的值 */
    int ivalueInLogLine = 0;
    char* svalueInLogLine = NULL;
    protoNode_t* node = NULL;

    node = (protoNode_t*)g_hash_table_lookup(parseTable, key);
    if(node == NULL) {
        logErr("%s not existed in hash table\n", key);
        return FALSE;
    }

    if(node->protoType != valuenode->type) {
        logErr("%s: protType mismatched(%d->%d)\n", key, node->protoType, valuenode->type);
        return FALSE;
    }

    if(valuenode->type == STTYPE_INTEGER) {
        ivalue = valuenode->value;
        ivalueInLogLine = node->value;
        return integerTest(op, ivalue, ivalueInLogLine);
    } else {
        svalue = valuenode->data;
        svalueInLogLine = node->data;
        return stringTest(op, svalue, svalueInLogLine);
    }

    
    return FALSE;
}

gboolean testNode(stnode_t* node, GHashTable *parseTable)
{
    gboolean ret = FALSE;

    if(node->type == STTYPE_TEST)
    {
        test_t	*test = (test_t*)stnode_data(node);
        switch(test->op)
        {
            case TEST_OP_EQ:
            case TEST_OP_NE:
            case TEST_OP_GT:
            case TEST_OP_GE:
            case TEST_OP_LT:
            case TEST_OP_LE:
                ret = cmpPattern(test->val1, test->val2, test->op, parseTable);
            break;
            case TEST_OP_AND:
                if(testNode(test->val1, parseTable) == TRUE)
                {
                    if(testNode(test->val2, parseTable) == TRUE)
                    {
                        ret = TRUE;
                    }
                    else
                    {
                        ret = FALSE;
                    }
                }
                else
                {
                    ret = FALSE;
                }
            break;
            case TEST_OP_OR:
                if(testNode(test->val1, parseTable) == TRUE)
                {
                    ret = TRUE;
                }
                else
                {
                    if(testNode(test->val2, parseTable) == TRUE)
                    {
                        ret = TRUE;
                    }
                    else
                    {
                        ret = FALSE;
                    } 
                }
            break;
            default:
            break;
        }
    }
    else
    {
        // 数据节点直接返回TRUE
        return TRUE;
    }

    return ret;
}

gboolean doFilter(tfilter_t* tf, GHashTable *parseTable)
{
    if(tf->stRoot == NULL) return TRUE;

    return testNode(tf->stRoot, parseTable);
}



#define tfGetText() state.quotedString->str

gboolean parse(tfilter_t *tf, int token, char *text, stnode_t *node)
{
    gboolean ret = TRUE;
    switch(token)
    {
        case TOKEN_LPAREN:
		case TOKEN_RPAREN:
		case TOKEN_TEST_EQ:
		case TOKEN_TEST_NE:
		case TOKEN_TEST_GT:
		case TOKEN_TEST_GE:
		case TOKEN_TEST_LT:
		case TOKEN_TEST_LE:
		case TOKEN_TEST_AND:
		case TOKEN_TEST_OR:
		break;
        case TOKEN_UNPARSED:
        {
            gboolean exist = FALSE;
            exist = protoExist(text);
            if(exist)
            {
                set_lval(node, TOKEN_FIELD, text);
            }
            else
            {
                set_lval(node, TOKEN_STRING, text);
            }
        }
        break;
        case TOKEN_STRING:
            set_lval(node, TOKEN_STRING, text);
        break;
        case TOKEN_INTEGER:
            ret = (set_lval_int(tf, node, TOKEN_INTEGER, text) >= 0);
        break;
        default:
            ret = FALSE;
            g_assert_not_reached();
        break;
    }

    return ret;
}   

#define ALL_PATTERN "all"

tfilter_t* compile1(char* pattern);

tfilter_t* compile(char* pattern)
{
    if(strcmp(pattern, ALL_PATTERN) == 0) {
        tfilter_t *tf = NULL;
        tf = tfilter_new();
        tf->stRoot = NULL; // 表示不过滤信息
        tf->errorMessage = NULL;
        return tf;
    } else {
        return compile1(pattern);
    }
}

tfilter_t* compile1(char* pattern)
{
    // Set up the scanner
    yyscan_t scanner;
    YY_BUFFER_STATE in_buffer;
    tfScannerState_t state;
    tfilter_t *tf;
    gboolean failure = FALSE;
  
    tf_lex_init(&scanner);

    // Set up the parser
    void* Parser = TfilterAlloc(g_malloc);

    //char* pattern = "type != \"tcp\" && 200 >= port";
    // Do it!
    in_buffer = tf__scan_string(pattern, scanner);

    tf = tfilter_new();
    state.ptf = tf;
	state.quotedString = g_string_sized_new(strlen(pattern) + 1);
    
    tf_set_extra(&state, scanner);

    int lexToken;
    char* lexText = NULL;
    stnode_t* lexNode = NULL;

    while(1) {

        lexNode = stnode_new(STTYPE_UNINITIALIZED, NULL);
        lexToken = tf_lex(scanner);
        if (lexToken == 0) break;
        if (-1 == lexToken) {
            failure = TRUE;
            fprintf(stderr, "The scanner encountered an error.\n");
            break;
        }
        //lexText = tf_get_text(scanner);
        lexText = tfGetText();
        
        if(parse(tf, lexToken, lexText, lexNode) == FALSE)
        {
            failure = TRUE;
            break;
        }
    
    #if 0
        if(lexNode->type == STTYPE_INTEGER)
            printf("Token: %d Text:(m:%x t:%d i:%d)\n", lexToken, lexNode->magic, lexNode->type, lexNode->value);
        else
            printf("Token: %d Text:(m:%x t:%d d:%s)\n", lexToken, lexNode->magic, lexNode->type, (char*)lexNode->data);
    #endif
        Tfilter(Parser, lexToken, lexNode, tf);
    } 

    if (lexNode) {
		stnode_free(lexNode);
		lexNode = NULL;
	}

    Tfilter(Parser, 0, NULL, tf);
    
    if (tf->syntaxError)
    {
        //printf("syntax_error\n");
		failure = TRUE;
    }
    if (state.quotedString != NULL)
		g_string_free(state.quotedString, TRUE);

    tf__delete_buffer(in_buffer, scanner);
    // Cleanup the scanner and parser
    tf_lex_destroy(scanner);
    if (failure)
		goto FAILURE;

    TfilterFree(Parser, g_free);

    //tfilter_free(tf);
    return tf;

FAILURE:
    if (tf) {
        printf("%s\n", tf->errorMessage);
		g_free(tf->errorMessage);
		tfilter_free(tf);
	}

    return NULL;
}

int cleanFilter(tfilter_t* tf)
{
    if (tf) {
        if(tf->errorMessage)
		    g_free(tf->errorMessage);
		tfilter_free(tf);
	}

    return OP_OK;
}

static int
set_lval(stnode_t* node, int token, gpointer data)
{
	sttype_id_t	type_id = STTYPE_UNINITIALIZED;

	switch (token) {
		case TOKEN_STRING:
			type_id = STTYPE_STRING;
			break;
		case TOKEN_FIELD:
			type_id = STTYPE_FIELD;
			break;
		case TOKEN_UNPARSED:
			type_id = STTYPE_UNPARSED;
			break;
		default:
			g_assert_not_reached();
	}
	stnode_init(node, type_id, data);
	return token;
}

static int
set_lval_int(tfilter_t *tf, stnode_t* node, int token, char *s)
{
	sttype_id_t	type_id = STTYPE_UNINITIALIZED;
	gint32		val;

	if (!str_to_gint32(tf, s, &val)) {
		return SCAN_FAILED;
	}

	switch (token) {
		case TOKEN_INTEGER:
			type_id = STTYPE_INTEGER;
			break;
		default:
			g_assert_not_reached();
	}

	stnode_init_int(node, type_id, val);
	return token;
}

static gboolean
str_to_gint32(tfilter_t *tf, char *s, int* pint)
{
	char    *endptr;
	long	integer;

	errno = 0;
	integer = strtol(s, &endptr, 0);

	if (errno == EINVAL || endptr == s || *endptr != '\0') {
		/* This isn't a valid number. */
		tfilter_fail(tf, "\"%s\" is not a valid number.", s);
		return FALSE;
	}
	if (errno == ERANGE) {
		if (integer == LONG_MAX) {
			tfilter_fail(tf, "\"%s\" causes an integer overflow.", s);
		}
		else if (integer == LONG_MIN) {
			tfilter_fail(tf, "\"%s\" causes an integer underflow.", s);
		}
		else {
			/*
			 * XXX - can "strtol()" set errno to ERANGE without
			 * returning LONG_MAX or LONG_MIN?
			 */
			tfilter_fail(tf, "\"%s\" is not an integer.", s);
		}
		return FALSE;
	}
	if (integer > G_MAXINT32) {
		/*
		 * Fits in a long, but not in a gint32 (a long might be
		 * 64 bits).
		 */
		tfilter_fail(tf, "\"%s\" causes an integer overflow.", s);
		return FALSE;
	}
	if (integer < G_MININT32) {
		/*
		 * Fits in a long, but not in a gint32 (a long might be
		 * 64 bits).
		 */
		tfilter_fail(tf, "\"%s\" causes an integer underflow.", s);
		return FALSE;
	}

	*pint = (gint32)integer;
	return TRUE;
}

