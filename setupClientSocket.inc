#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <netdb.h>
#include <unistd.h>     /* for close() */
#include <string.h>		/* for memset() */
#include <assert.h>		/* for assert() */

typedef enum {SOCKET_TYPE_TCP, SOCKET_TYPE_UDP} SocketType;

//assume TCP unless isUDP is true
int setupClientSocket(char* serverHost, char* serverPort, SocketType type) {
	assert(serverHost != NULL);
	//skip http:// or https://
	if(strstr(serverHost, "http://") == serverHost)
		serverHost += 7;
	else if(strstr(serverHost, "https://") == serverHost)
		serverHost += 8;

	struct addrinfo addrCriteria;
	memset(&addrCriteria, 0, sizeof(addrCriteria));
	addrCriteria.ai_family = AF_INET;
	if(type == SOCKET_TYPE_UDP) {
		addrCriteria.ai_socktype = SOCK_DGRAM;
		addrCriteria.ai_protocol = IPPROTO_UDP;
	}
	else if(type == SOCKET_TYPE_TCP) {
		addrCriteria.ai_socktype = SOCK_STREAM;
		addrCriteria.ai_protocol = IPPROTO_TCP;
	}
	else {
		fprintf(stderr, "unexpected SocketType in setupSocket()\n");
		return -1;
	}
	struct addrinfo *serverAddr;
	int error = getaddrinfo(serverHost, serverPort, &addrCriteria, &serverAddr);
	if(error != 0) {
		fprintf(stderr, "could not get address information for host\n");
		return -1;
	}

	int sock = -1;
	//loop through addresses and try to connect() to each one
	struct addrinfo* addr;
	for(addr = serverAddr; addr != NULL; addr = addr->ai_next) {
		sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if(sock < 0) {
			//socket creation failed -- try next address
			continue;
		}
		if(connect(sock, addr->ai_addr, addr->ai_addrlen) == 0) {
			//socket connection succeeded
			break;
		}
		//socket connection failed -- try next address
		close(sock);
		sock = -1;
	}

	freeaddrinfo(serverAddr);
	
	return sock;
}