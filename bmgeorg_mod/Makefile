CC = gcc
CFLAGS = -Wall -g
PROGS =	robotClient robotServer dummyServer
DEPS = utility.h utility.c Makefile setupClientSocket.inc

all: $(PROGS)

robotClient: client.c clientMessenger.h clientMessenger.c $(DEPS)
	${CC} -o $@ client.c clientMessenger.c utility.c ${CFLAGS}

robotServer: server.c serverMessenger.h serverMessenger.c $(DEPS)
	${CC} -o $@ server.c serverMessenger.c utility.c ${CFLAGS} -pthread

clean:
	rm -f ${PROGS}
