#ifndef __FILTER_H__
#define __FILTER_H__
#include <glib.h>
#include "syntax-tree.h"

#define PROTO_CONF_PATH "/home/zhangyao/cwork/logserver/proto.conf"

typedef struct {
	/* Syntax Tree stuff */
	stnode_t	*stRoot;
	gboolean	syntaxError;
	gchar		*errorMessage;
} tfilter_t;

typedef struct {
	tfilter_t *ptf;
	GString* quotedString;
} tfScannerState_t;

/* Return value for error in scanner. */
#define SCAN_FAILED	-1	/* not 0, as that means end-of-input */

/* Set dfw->error_message */
void tfilter_fail(tfilter_t *tf, const char *format, ...) G_GNUC_PRINTF(2, 3);

tfilter_t* compile(char* pattern);
int cleanFilter(tfilter_t* tf);
gboolean doFilter(tfilter_t* tf, GHashTable *parseTable);


#endif

