#define _GNU_SOURCE

#include "serverMessenger.h"
#include "utility.h"
#include "setupClientSocket.inc"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <string.h>     /* for memset() */
#include <netinet/in.h> /* for in_addr */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <netdb.h>      /* for getHostByName() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <unistd.h>     /* for close() */
#include <time.h>       /* for time() */
#include <signal.h>
#include <pthread.h>

#define MAXLINE 1000
#define TRUE 1
#define FALSE 0

//Below added - Ben
#define DEBUG


typedef struct queuePtr {
	char* robotComm;
	int port; //port to send message to 
	int commandCode; // 0 - 6 mapped to different client commands
	double waitTime; //NULL if not used


	char* msgbody;
	char* msglen;
	char* msgtype;
} queueElem_t;

/** List node  **/
typedef struct queueNode {
	queueElem_t *queuePtr;               /* Pointer to queue record         */
	struct queueNode *next;        /* Pointer to next node          */
	struct queueNode *prev;        /* Pointer to next node          */
} queueNode_t;

/** Car list **/
typedef struct queueList {
	queueNode_t *head;
	queueNode_t *tail;
} queue_t;

queue_t *toRobot;
queue_t *toClient;
queue_t *parseToClient;

void enqueue(queue_t * queue, queueElem_t *elem);
queueElem_t * dequeue(queue_t * queue);
int isEmpty(queue_t * queue);
void *sendRecvRobot();
void *sendToClient();

//Method Signatures
char* getRobotID(char* msg);
uint32_t getRequestID(char* msg);
char* getRequestStr(char* msg);
char* generateHTTPRequest(char* robotAddress, char* robotID, char* requestStr, char* imageID);
char* getRobotPortForRequestStr(char* requestStr);
void flushBuffersAndExit();

/*Experimental*/

typedef struct {
	char* robotComm;
	int port; //port to send message to 
	int commandCode; // 0 - 6 mapped to different client commands
	double waitTime; //NULL if not used
} command;

typedef struct {
	char* msgbody;
	char* msglen;
	char* msgtype;
} clientMsg;
/*Experimental*/

/*
3 queues will be used to generate tasks


*/






//Main Method
int main(int argc, char *argv[])
{
	if(argc != 5) {
		quit("Usage: %s <server_port> <robot_IP/robot_hostname> <robot_ID> <image_id>", argv[0]);
	}
	//read args
	unsigned short localUDPPort = atoi(argv[1]);
	char* robotAddress = argv[2];
	char* robotID = argv[3];
	char* imageID = argv[4];
	
	plog("Read arguments");
	plog("Robot address: %s", robotAddress);
	plog("Robot ID: %s", robotID);
	plog("Image ID: %s", imageID);
	
	//listen for ctrl-c and call flushBuffersAndExit()
	signal(SIGINT, flushBuffersAndExit);

	//Create socket for talking to clients
	int clientSock;
	if((clientSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		quit("could not create client socket - socket() failed");
	}
	
	plog("Created client socket: %d", clientSock);
		
	//Construct local address structure for talking to clients
	struct sockaddr_in localAddress;
	memset(&localAddress, 0, sizeof(localAddress));		//Zero out structure
	localAddress.sin_family = AF_INET;					//Any Internet address family
	localAddress.sin_addr.s_addr = htonl(INADDR_ANY);	//Any incoming interface
	localAddress.sin_port = htons(localUDPPort);		//The port clients will sendto

	if(bind(clientSock, (struct sockaddr *) &localAddress, sizeof(localAddress)) < 0) {
		quit("could not bind to client socket - bind() failed");
	}
	
	plog("binded to client socket");



        toRobot = malloc(sizeof(queue_t));
        toClient = malloc(sizeof(queue_t));

	pthread_t t1, t2;

	pthread_create(&t1, NULL, sendRecvRobot, NULL);
	pthread_create(&t2, NULL, sendToClient, NULL);


	//Loop for each client request
	for(;;) {
		plog("Start loop to handle client request");
	
		//Receive request from client
		struct sockaddr_in clientAddress;
		unsigned int clientAddressLen = sizeof(clientAddress);	//in-out parameter
		char clientBuffer[MAXLINE+1];	//Buffer for incoming client requests
		memset(clientBuffer, 0, MAXLINE+1);
		
		int recvMsgSize;
		//Block until receive a guess from a client
		if((recvMsgSize = recvfrom(clientSock, clientBuffer, MAXLINE, 0,
				(struct sockaddr *) &clientAddress, &clientAddressLen)) < 0) {
			quit("could not receive client request - recvfrom() failed");
		}
		
		plog("Received request of %d bytes", recvMsgSize);

		//Interpret client request
		char* requestRobotID = getRobotID(clientBuffer);
		if(strcmp(robotID, requestRobotID) != 0) {
			fprintf(stderr, "invalid request - robot ID's don't match\n");
			continue;
		}
		
		plog("Requested robot ID: %s", requestRobotID);

		char* requestStr = getRequestStr(clientBuffer);
		char* robotPort = getRobotPortForRequestStr(requestStr);
		
		plog("Request string: %s", requestStr);
		plog("Calculated port: %s", robotPort);
		
		//Send HTTP request to robot
		int robotSock;
		if((robotSock = setupClientSocket(robotAddress, robotPort, SOCKET_TYPE_TCP)) < 0) {
			quit("could not connect to robot");
		}	
		
		plog("Set up robot socket: %d", robotSock);
		
		char* httpRequest = generateHTTPRequest(robotAddress, robotID, requestStr, imageID);
		
		plog("Created http request: %s", httpRequest);
		
		if(write(robotSock, httpRequest, strlen(httpRequest)) != strlen(httpRequest)) {
			quit("could not send http request to robot - write() failed");
		}
		
		plog("Sent http request to robot");
		
		free(httpRequest);
		
		plog("freed http request");

		//Read response from Robot
		int pos = 0;
		char* httpResponse = malloc(MAXLINE);
		int n;
		char recvLine[MAXLINE+1]; //holds one chunk of read data at a time
		while((n = read(robotSock, recvLine, MAXLINE)) > 0) {
			memcpy(httpResponse+pos, recvLine, n);
			pos += n;
			httpResponse = realloc(httpResponse, pos+MAXLINE);
		}

		plog("Received http response of %d bytes", pos);
		plog("http response: ");
		#ifdef DEBUG
		int j;
		for(j = 0; j < pos; j++)
			fprintf(stderr, "%c", httpResponse[j]);
		#endif
		
		//Parse Response from Robot
		char* httpBody = strstr(httpResponse, "\r\n\r\n");
        int httpBodyLength
        if(httpBody == NULL)
        {
            httpBody = httpResponse;
            httpBodyLength = 0;
        }
        else
        {
            httpBody += 4;
            httpBodyLength = (httpResponse + pos) - httpBody;
        }
		
		plog("http body of %d bytes", httpBodyLength);
		plog("http body: ");
		#ifdef DEBUG
		for(j = 0; j < httpBodyLength; j++)
			fprintf(stderr, "%c", httpBody[j]);
		#endif
		
		//Send response back to the UDP client
		uint32_t requestID = getRequestID(clientBuffer);
		sendResponse(clientSock, &clientAddress, clientAddressLen, requestID, httpBody, httpBodyLength);
		
		plog("sent http body response to client");
		
		free(httpResponse);
		
		plog("freed http response");
		plog("End loop to handle client request");
	}
}

char* getRobotID(char* msg) {
	return msg+4;
}

uint32_t getRequestID(char* msg) {
	return ntohl(*((uint32_t*)msg));
}

char* getRequestStr(char* msg) {
	char* robotID = getRobotID(msg);

	return msg+5+strlen(robotID);
}

char* generateHTTPRequest(char* robotAddress, char* robotID, char* requestStr, char* imageID) {
	char* URI = malloc(MAXLINE);
	memset(URI, 0, MAXLINE);
	double x;
	if(sscanf(requestStr, "MOVE %lf", &x) == 1) {
		sprintf(URI, "/twist?id=%s&lx=%lf", robotID, x);
	}
	else if(sscanf(requestStr, "TURN %lf", &x) == 1) {
		sprintf(URI, "/twist?id=%s&az=%lf", robotID, x);
	}
	else if(strstr(requestStr, "STOP") != NULL) {
		sprintf(URI, "/twist?id=%s&lx=0",robotID);
	}
	else if(strstr(requestStr, "GET IMAGE") != NULL) {
		sprintf(URI, "/snapshot?topic=/robot_%s/image&width=600&height=500", imageID);
	}
	else if(strstr(requestStr, "GET GPS") != NULL) {
		sprintf(URI, "/state?id=%s",robotID);
	}
	else if(strstr(requestStr, "GET DGPS") != NULL) {
		sprintf(URI, "/state?id=%s",robotID);
	}
	else if(strstr(requestStr, "GET LASERS") != NULL) {
		sprintf(URI, "/state?id=%s",robotID);
	}
	else {
		free(URI);
		return NULL;
	}

	char* httpRequest = malloc(MAXLINE);
	memset(httpRequest, 0, MAXLINE);
	strcat(httpRequest, "GET ");
	strcat(httpRequest, URI);
	strcat(httpRequest, " HTTP/1.1\r\n");
	
	//Host
	strcat(httpRequest, "Host: ");
	strcat(httpRequest, robotAddress);
	strcat(httpRequest, ":");
	strcat(httpRequest, getRobotPortForRequestStr(requestStr));
	strcat(httpRequest, "\r\n");
	
	//Connection: close
	strcat(httpRequest, "Connection: close\r\n");
	
	strcat(httpRequest, "\r\n");

	free(URI);
	return httpRequest;
}

//Gets TCP Port
char* getRobotPortForRequestStr(char* requestStr) {
	if(strstr(requestStr, "MOVE") != NULL) {
		return "8082";
	} else if(strstr(requestStr, "TURN") != NULL) {
		return "8082";
	} else if(strstr(requestStr, "STOP") != NULL) {
		return "8082";
	} else if(strstr(requestStr, "GET IMAGE") != NULL) {
		return "8081";
	} else if(strstr(requestStr, "GET GPS") != NULL) {
		return "8082";
	} else if(strstr(requestStr, "GET DGPS") != NULL) {
		return "8084";
	} else if(strstr(requestStr, "GET LASERS") != NULL) {
		return "8083";
	} else {
		return NULL;
	}
}

/* This routine contains the data printing that must occur before the program 
*  quits after the CNTC signal. */
void flushBuffersAndExit() {
	fflush(stdout);
	exit(0);
}

void *sendRecvRobot() {
	while(1) {
		if (isEmpty(toRobot) == FALSE) {
			queueElem_t *elem = dequeue(toRobot);
			sleep(1);
			free(elem->msgbody);
			elem->msgbody = malloc(sizeof(char) * 8);
			strcpy(elem->msgbody, "Recvd");

			enqueue(toClient, elem);
		} else {
			pthread_yield();
		}
	}
return NULL;
}

void *sendToClient() {
	while(1) {
		if (isEmpty(toClient) == FALSE) {
			queueElem_t *elem = dequeue(toClient);
			printf("Code is %d, Received = %s\n", elem->commandCode, elem->msgbody);
		} else {
			pthread_yield();
		}

	}
return NULL;
}



//Queue Code.

//Enqueue takes a queueElem_t and adds it to the tail end of the queue
void enqueue(queue_t * queue, queueElem_t *elem)
{
	queueNode_t *Node = malloc(sizeof(queueNode_t));
	Node->queuePtr = elem;
	if( queue->tail == NULL ) {
		queue->head = Node;
		queue->tail = Node;
		Node->next = NULL;
		Node->prev = NULL;
	} else {
		Node->prev = queue->tail;
		Node->next = NULL;
		queue->tail->next = Node;
		queue->tail = Node;
	}
}

//Dequeue removes the head of the queue and returns the queueElem_t it held

queueElem_t * dequeue(queue_t * queue)
{
	if(queue->head == NULL) {
		return NULL;
	} else if (queue->head->next == NULL) {
		queueNode_t *tempNode = queue->head;
		queue->head = NULL;
		queue->tail = NULL;
		return tempNode->queuePtr;
	} else {
		queueNode_t *tempNode = queue->head;
		queue->head->next->prev = NULL;
		queue->head = queue->head->next;
		return tempNode->queuePtr;
	}
}

int isEmpty(queue_t * queue)
{
	if (queue->head == NULL) {
		return TRUE;
	} else {
		return FALSE;
	}

}

