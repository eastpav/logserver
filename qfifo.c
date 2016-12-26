/* qfifo.c - buffer fifo manager source file */

#include <unistd.h>
#include <stdlib.h>
#include "qfifo.h"

/* init a fifo queue */
int qfifoInit( qfifo_t *pfifo )
{
	int err;
	
	if ( pfifo == NULL ) return -1;

	err = pipe( pfifo->pipes );
	pfifo->count = 0;

	return err;
}

/* cleanup a fifo queue */
void qfifoCleanup( qfifo_t *pfifo )
{
	if ( pfifo == NULL ) return;

	/* close pipe now */
	(void)close( pfifo->pipes[0] );
	(void)close( pfifo->pipes[1] );
}

/* create a fifo queue */
qfifo_t* qfifoCreate( void )
{
	qfifo_t* pfifo = (qfifo_t*)	malloc( sizeof(qfifo_t));
	if ( pfifo )
	{
		if ( qfifoInit(pfifo) )
		{
			free( pfifo );
			pfifo = NULL;
		}
	}
	
	return pfifo;
}

/* destroy a fifo queue */
void qfifoDestroy( qfifo_t *pfifo )
{
	qfifoCleanup( pfifo );
	free (pfifo);
}

/* put string point by ptr of len to fifo */
int qfifoPut( qfifo_t *pfifo, char* ptr, int len )
{
	int nwrite= 0;

	nwrite = write( pfifo->pipes[1], ptr, len);
	if ( nwrite != len ) {
		return -1;
	}
	pfifo->count += len;

    return 0;
}

int qfifoGet( qfifo_t *pfifo, char* ptr, int len )
{
	int nread = 0;

	nread = read( pfifo->pipes[0], ptr, len );
	if( nread != len ) {
		//Log_write(NULL, 1, LOG_LEVEL_ERR, "qfifo_put() read faild\n");
		return -1;
	}
	pfifo->count -= len;
	if ( pfifo->count < 0  )
		pfifo->count = 0;
		
	return 0;
}

int qfifoCount( qfifo_t *pfifo )
{
	return pfifo->count;
}


