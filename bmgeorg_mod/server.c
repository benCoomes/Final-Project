/*
Names: Jonathan Sarasua, Joshua Dunster, Ben Coomes, Megan Fowler

Layout: The code is split into three parts. The main function takes in commands
	from a client, parses them for necessary pieces, and places them in a 
	queue for the sendRecvRobot function to handle. sendRecvRobot handles
	communication with the robot. When it gets responses back, minimal
	parsing occurs before adding it to another queue for sending back to the
	client. The sendToClient function takes these elements and sends them
	on to the client. All three of these functions are threaded, so they are
	all running at the same time.
*/


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
#include <math.h>
#include <sys/time.h>

#define MAXLINE 1400
#define TRUE 1
#define FALSE 0
#define READY 1
#define NOTREADY 0

/*
finalIndex and readyForNextPacket are used to tell when the server is ready
to receive more messages.
*/
int finalIndex;
int readyForNextPacket = NOTREADY;

/*
The following three structs make up the queue. queueList and queueNode are the 
skeleton of the queue while queueElem contains the data.
*/
typedef struct queueElem {
	char* robotCommand;	//Command to send to robot--Not parsed
	uint32_t ID; //ID of client

	struct sockaddr_in * clientAddress;
	unsigned int clientAddressLen;
	int clientSock;

	int commandIndex; // indicates order that command was recieved in
	char* msgbody; //Body of message to client
	double msglen; //Length of message to client
} queueElem_t;

/** List node  **/
typedef struct queueNode {
	queueElem_t *queueElem;
	struct queueNode *next;
	struct queueNode *prev;
} queueNode_t;

/** Queue list **/
typedef struct queueList {
	queueNode_t *head;
	queueNode_t *tail;
} queue_t;

//Queues for communication between threads
queue_t *toRobot;
queue_t *toClient;

void enqueue(queue_t * queue, queueElem_t *elem);
queueElem_t * dequeue(queue_t * queue);
int isEmpty(queue_t * queue);
void *sendRecvRobot();
void *sendToClient();

//Method Signatures
char* getRobotID(char* msg);
uint32_t getRequestID(char* msg);
char* getRequestStr(queueElem_t msg);
char* generateHTTPRequest(char* robotAddress, char* robotID, char* requestStr, char* imageID);
char* getRobotPortForRequestStr(char* requestStr);
void flushBuffersAndExit();
double getTime();


/*
These globals are variable that will be used by all three threads.
*/
unsigned short localUDPPort;
char* robotAddress;
char* robotID;
char* imageID;
pthread_mutex_t qMutex = PTHREAD_MUTEX_INITIALIZER;

//Main Method
int main(int argc, char *argv[])
{
	if(argc != 5) {
		quit("Usage: %s <server_port> <robot_IP/robot_hostname> <robot_ID> <image_id>", argv[0]);
	}
	//read args
	localUDPPort = atoi(argv[1]);
	robotAddress = argv[2];
	robotID = argv[3];
	imageID = argv[4];

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


	/*Create the queues*/
        toRobot = malloc(sizeof(queue_t));
        toClient = malloc(sizeof(queue_t));

	/*Create the threads*/
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
		//Wait until we receive message
		if((recvMsgSize = recvfrom(clientSock, clientBuffer, MAXLINE, 0,
				(struct sockaddr *) &clientAddress, &clientAddressLen)) < 0) {
			quit("could not receive client request - recvfrom() failed");
		}
		/*Checking Header*/
		printf("printing header\n");
		int i = 0;
		while(i < 16) {
			printf("%u ",(uint8_t)*(clientBuffer+i));
			i++;
		}
		/*Header Check*/
		plog("Received request of %d bytes", recvMsgSize);

		//This size will be decremented as we pare through buffer
		//Once it reaches the end, we know we're done with message.
		//12 is the size of the header before robot ID
		int size = recvMsgSize - 12;

		//Interpret client request
		char* requestRobotID = getRobotID(clientBuffer);
		if(strcmp(robotID, requestRobotID) != 0) {
			fprintf(stderr, "invalid request - robot ID's don't match\n");
			continue;
		}

		char* toRobotPtr = clientBuffer + 12; //Move ptr to robot ID
		uint32_t ID = getRequestID(clientBuffer);
		while(*toRobotPtr != 0){
			toRobotPtr++;
			size--;
		}
		toRobotPtr++;
		size--;

        	readyForNextPacket = NOTREADY;
        	int commandIndex = 0;

		//There is usually a null character left over at the end, so 
		//we loop until toRobotPtr is only pointing to it.

		//This loop handles enqueuing all of the commands for the 
		//sendRecvRobot function to handle
		while(size > 1) {
			int commandStringSize;



			commandStringSize = (unsigned short)*(toRobotPtr);
			toRobotPtr++;
			size--;

			queueElem_t * element = malloc(sizeof(queueElem_t));
			element->ID = ID;
			element->clientSock = clientSock;
			element->clientAddress = &clientAddress;
			element->clientAddressLen = clientAddressLen;
            element->commandIndex = commandIndex++;

			element->robotCommand = malloc(commandStringSize + 1);
			strncpy(element->robotCommand,toRobotPtr,commandStringSize);
			*(element->robotCommand + commandStringSize) = 0; //Add Null character to terminate string
			printf("Command String = %s\n",element->robotCommand);

			toRobotPtr+=(commandStringSize);
			size-=(commandStringSize);

			//Locks protect the code where threads may interfere with each other.
			pthread_mutex_lock(&qMutex);
			enqueue(toRobot,element);
			pthread_mutex_unlock(&qMutex);
		}
        	finalIndex = commandIndex -1;
		while(readyForNextPacket != READY) {
			sleep(1);
		}

	}
}

char* getRobotID(char* msg) {
	return msg+12;
}

uint32_t getRequestID(char* msg) {
	return ntohl(*((uint32_t*)msg));
}

char* getRequestStr(queueElem_t msg) {
	return msg.robotCommand;
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

/* This routine removes element from command queue and formats the http
  request to be sent. Then proceeds to send the message, and recieve a response.
  The response is then entered into a queue to be sent back to the client. */
void *sendRecvRobot() {
	while(1) {
		//Added for Megan
		double sleepTime;
		int waitSeconds;
		int waitUSeconds;
		double turnAngle;
		double turnSpeed;
		queueElem_t *elem;
		double timeSpent;


		if (isEmpty(toRobot) == FALSE) {
			pthread_mutex_lock(&qMutex);
			elem = dequeue(toRobot);						//dequeue command that is to be sent
			pthread_mutex_unlock(&qMutex);


			char* requestStr = getRequestStr(*elem);
			char* robotPort = getRobotPortForRequestStr(requestStr);

			plog("Request string: %s", requestStr);
			plog("Calculated port: %s", robotPort);

			//Send HTTP request to robot
			int robotSock;
			if((robotSock = setupClientSocket(robotAddress, robotPort, SOCKET_TYPE_TCP)) < 0) {
				quit("could not connect to robot");
			}

			char* httpRequest = generateHTTPRequest(robotAddress, robotID, requestStr, imageID);


			//calculate time that it take for message to be sent and received.
			timeSpent = getTime();

			if(write(robotSock, httpRequest, strlen(httpRequest)) != strlen(httpRequest)) {
				quit("could not send http request to robot - write() failed");
			}
			plog("Sent http request to robot");

			free(httpRequest);

			timeSpent = getTime() - timeSpent;


			//Calculate wait time (dist - time spent in sendRequest).
			if(strstr(requestStr, "MOVE") != NULL){
				double dist = requestStr[7] - 48; //7th character is the distance, -48 to convert from ascii
				printf("Dist = %f\n",dist);

				if(dist > timeSpent) {
					 sleepTime = dist - timeSpent;

					 plog("waiting for %lf seconds", sleepTime);

					 waitSeconds = (int) sleepTime;
					 sleepTime -= waitSeconds;
					 waitUSeconds = (int) (sleepTime*1000000);

					 //Wait until robot reaches destination.
					 sleep(waitSeconds);
					 usleep(waitUSeconds);
				}
			}

			//Calculate wait time (turnAngle/(M_PI/4) - time spent in sendRequest).
			else if (strstr(requestStr, "TURN") != NULL){
				const double actualSpeed = .89*M_PI/4;

				char* requestPtr = requestStr;
				int spaceCounter;
				while(spaceCounter != 2) {
					if(*requestPtr == ' ') {
						spaceCounter++;
					}
					requestPtr++;
				}

				sscanf(requestPtr,"TURN %lf %lf",&turnSpeed,&turnAngle);
				printf("Angle = %s\n",requestPtr);
				printf("Turn Angle = %lf\n", turnAngle);

				if(turnAngle/actualSpeed > timeSpent) {
					sleepTime = turnAngle/actualSpeed - timeSpent;

					plog("waiting for %lf seconds", sleepTime);

					waitSeconds = (int) sleepTime;
					sleepTime -= waitSeconds;
					waitUSeconds = (int) (sleepTime*1000000);

					//Wait until robot turns to correct orientation.
					sleep(waitSeconds);
					usleep(waitUSeconds);
				}
			}


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

			//Parse Response from Robot
			char* httpBody = strstr(httpResponse, "\r\n\r\n");
      int httpBodyLength;
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

			elem->msgbody = httpBody;
			elem->msglen = httpBodyLength;

			free(httpResponse);

			plog("freed http response");

      if(strstr(requestStr, "GET") != NULL){
				pthread_mutex_lock(&qMutex);
				enqueue(toClient, elem);
		  	pthread_mutex_unlock(&qMutex);
      }

      if(elem->commandIndex == finalIndex){
        readyForNextPacket = READY;
      }
			} else {
				pthread_yield();
			}
		}
	return NULL;
}

/* This routine removes the element from the queue to be sent to the client,
   then sends the message to the client when able. */
void *sendToClient() {
	while(1) {
		if (isEmpty(toClient) == FALSE) {
			pthread_mutex_lock(&qMutex);
			queueElem_t *elem = dequeue(toClient);
			pthread_mutex_unlock(&qMutex);
            
		    //Send response back to the UDP client
			uint32_t requestID = elem->ID; //may need to be included with struct
            int commandIndex = elem->commandIndex;
            int isImage = (strstr(elem->robotCommand, "IMAGE") == NULL ? 0 : 1);
			sendResponse(elem->clientSock, elem->clientAddress, elem->clientAddressLen, requestID, commandIndex, elem->msgbody, elem->msglen, isImage);
			
            plog("sent http body response to client");
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
	Node->queueElem = elem;
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
		return tempNode->queueElem;
	} else {
		queueNode_t *tempNode = queue->head;
		queue->head->next->prev = NULL;
		queue->head = queue->head->next;
		return tempNode->queueElem;
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

double getTime() {
   struct timeval curTime;
   (void) gettimeofday(&curTime, (struct timezone *)NULL);
   return (((((double) curTime.tv_sec) * 10000000.0)
      + (double) curTime.tv_usec) / 10000000.0);
}
