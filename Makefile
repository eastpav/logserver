CC := gcc
CFLAGS := $(CFLAGS) -Wall -g
CFLAGS  += `pkg-config --cflags glib-2.0`
LDFLAGS := `pkg-config --libs glib-2.0`
LDFLAGS += -pthread

SKFLAGS= $(RPM_OPT_FLAGS) -DSYSV -fomit-frame-pointer -Wall -fno-strength-reduce -DSYSLOG_INET
FSSTND = -DFSSTND
SYSLOGD_PIDNAME = -DSYSLOGD_PIDNAME=\"syslogd.pid\"
SYSLOGD_FLAGS= -DSYSLOG_INET -DSYSLOG_UNIXAF -DNO_SCCS ${FSSTND} \
	${SYSLOGD_PIDNAME}
DEB =

LOGD_OBJ := filter.o scanner.o grammar.o sttype-test.o syntax-tree.o proto.o semcheck.o 
LOGD_OBJ += syslogd.o dllist.o epollevent.o pidfile.o qfifo.o socketevent.o watchserver.o

WATCH_OBJ := logwatch.o socketevent.o

all: syslogd logwatch log

syslogd: $(LOGD_OBJ)
	$(CC)  $(LOGD_OBJ) $(LDFLAGS) -o syslogd

logwatch: $(WATCH_OBJ)
	$(CC)  $(WATCH_OBJ) $(LDFLAGS) -o logwatch

log: log.o
	$(CC) log.o -o log  

filter.o: filter.c grammar.h grammar.h scanner.h

sttype-test.o: sttype-test.h sttype-test.c

syntax-tree.o: syntax-tree.h syntax-tree.c

proto.o: proto.h proto.c
semcheck.o: semcheck.h semcheck.c

grammar.o: grammar.h grammar.c

grammar.h grammar.c: grammar.y
	lemon grammar.y

scanner.o: scanner.h

scanner.h: scanner.l
	flex --outfile=scanner.c --header-file=scanner.h scanner.l

dllist.o: dllist.h

epollevent.o: epollevent.h

pidfile.o: pidfile.h

qfifo.o: qfifo.h

socketevent.o: 

watchserver.o: watchserver.h

syslogd.o: syslogd.c
	${CC} ${CFLAGS} ${SKFLAGS} ${SYSLOGD_FLAGS} $(DEB) -c syslogd.c

log.o: log.c

# Prevent yacc from trying to build parsers.
# http://stackoverflow.com/a/5395195/79202
%.c: %.y


.PHONY: clean
clean:
	rm -f *.o
	rm -f scanner.c scanner.h
	rm -f grammar.c grammar.h grammar.out
	rm -f syslogd
	rm -f logwatch
	rm -f log
