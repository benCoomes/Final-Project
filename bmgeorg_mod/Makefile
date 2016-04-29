CC = gcc
CXX = g++
CFLAGS = -Wall -g
PROGS =	robotClient robotServer
DEPS = utility.h utility.c Makefile setupClientSocket.inc
OBJS = compression.o server.o client.o utility.o clientMessenger.o serverMessenger.o

all:
	make compile

compile:
	make ${OBJS}
	make ${PROGS}

robotClient: ${OBS} 
	${CXX} -o $@ client.o compression.o clientMessenger.o utility.o -Wall

robotServer: ${OBJS}
	${CXX} -o $@ server.o compression.o serverMessenger.o utility.o -Wall -pthread

utility.o: utility.h utility.c
	${CC} utility.c ${CFLAGS} -c

clientMessenger.o: clientMessenger.c clientMessenger.h
	${CC} -o $@ clientMessenger.c ${CFLAGS} -c

serverMessenger.o: serverMessenger.c serverMessenger.h
	${CC} -o $@ serverMessenger.c ${CFLAGS} -c

client.o: client.c
	${CC} -o $@ client.c ${CFLAGS} -c

server.o: server.c
	${CC} -o $@ server.c ${CFLAGS} -c -pthread

compression.o: Compression.cpp
	${CXX} -c -o $@ Compression.cpp

clean:
	rm -f ${OBJS} ${PROGS}
