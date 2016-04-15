#include "clientMessenger.h"
#include "utility.h"
#include "setupClientSocket.inc"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>	//for uint32_t
#include <sys/time.h>	//for setitimer()
#include <string.h>     //for memset()
#include <sys/socket.h> //for socket(), connect(), sendto(), and recvfrom()
#include <unistd.h>     //for close()
#include <netdb.h>		//for addrinfo
#include <signal.h>		//for signal
#include <assert.h>		//for assert()

const int RESPONSE_MESSAGE_SIZE = 1000;

//UDP socket
int sock = -1;
char* robotID;

//private functions
void setTimer(double seconds);
void stopTimer();
void timedOut(int ignored);

void* recvMessage(int ID, int* messageLength);
int extractMessageID(void* message);
int extractNumMessages(void* message);
int extractSequenceNum(void* message);

void setupMessenger(char* serverHost, char* serverPort, char* _robotID) {
	assert(serverHost != NULL);
	assert(serverPort != NULL);
	assert(_robotID != NULL);
	
	sock = setupClientSocket(serverHost, serverPort, SOCKET_TYPE_UDP);	
	if(sock < 0)
		quit("could not set up socket to server");
	robotID = _robotID;
}

//the alarm handler for the timeout alarm
void timedOut(int ignored) {
	quit("Timed out without receiving server response");
}

/*
	Send request, receive and reassemble response
	Exit if you don't receive entire response after timeout seconds
	Set responseLength to total length of response
	Return pointer to response data
*/
void* sendRequest(char* requestString, int* responseLength, double timeout) {
	static uint32_t ID = 0;
	
	//4 bytes for ID + robotID length + 1 byte for null char + requestString length + 1 byte for null char
	int requestLen = 4+strlen(robotID)+1+strlen(requestString)+1;
	void* request = malloc(requestLen);
	
	//insert ID
	*((uint32_t*)request) = htonl(ID);
	
	//insert robotID string
	memcpy(((char*)request)+4, robotID, strlen(robotID)+1);
	
	//insert request string
	memcpy(((char*)request)+4+strlen(robotID)+1, requestString, strlen(requestString)+1);
	
	//send request
	int numBytesSent = send(sock, request, requestLen, 0);
	if(numBytesSent < 0)
		quit("could not send request - send() failed");
	else if(numBytesSent != requestLen)
		quit("send() didn't send the whole request");
	
	free(request);

	//start timeout timer
	setTimer(timeout);
	
	//get first message so we can allocate space for all messages
	int messageLength;
	void* message = recvMessage(ID, &messageLength);
	int numMessages = extractNumMessages(message);
	int i = extractSequenceNum(message);
	
	void** messages = malloc(sizeof(void*)*numMessages);
	memset(messages, 0, sizeof(void*)*numMessages);
	
	messages[i] = message;
	
	//only the last message's length is important
	//the other lengths we assume to be equal to RESPONSE_MESSAGE_SIZE
	int lastMessageLength = numMessages == 1? messageLength : 0;

	plog("ID: %d", extractMessageID(message));
	plog("Total: %d", extractNumMessages(message));
	plog("Sequence: %d", extractSequenceNum(message));
	plog("Length: %d", messageLength);

	int numReceived = 1;
	//reassemble response messages
	while(numReceived < numMessages) {
		message = recvMessage(ID, &messageLength);
		i = extractSequenceNum(message);

		plog("ID: %d", extractMessageID(message));
		plog("Total: %d", extractNumMessages(message));
		plog("Sequence: %d", extractSequenceNum(message));
		plog("Length: %d", messageLength);

		if(((char*)messages[i]) == NULL) {
			messages[i] = message;
			numReceived++;
			
			//if last message, record length
			if(i == numMessages-1)
				lastMessageLength = messageLength;
		} else {
			plog("duplicate message");
			free(message);
		}
	}
	//fprintf(stderr, "Last message length: %d\n", lastMessageLength);
	
	//stop timer
	stopTimer();
	
	int PAYLOAD_SIZE = RESPONSE_MESSAGE_SIZE - 12;
	*responseLength = (numMessages-1)*PAYLOAD_SIZE + (lastMessageLength-12);
	void* fullResponse = malloc(*responseLength);
	for(i = 0; i < numMessages-1; i++) {
		memcpy(((char*)fullResponse)+i*PAYLOAD_SIZE, ((char*)messages[i])+12, PAYLOAD_SIZE);
		free(messages[i]);
	}
	//copy last message data
	memcpy(((char*)fullResponse)+(numMessages-1)*PAYLOAD_SIZE, ((char*)messages[numMessages-1])+12, lastMessageLength-12);
	free(messages[numMessages-1]);

	//update ID for next call
	ID++;
	
	free(messages);
	
	plog("Full response length: %d", *responseLength);
	
	return fullResponse;
}

void setTimer(double seconds) {
	struct itimerval it_val;

	if(signal(SIGALRM, (void (*)(int)) timedOut) == SIG_ERR) {
		quit("could not set signal handler - signal() failed");
	}

	it_val.it_value.tv_sec =  (int)seconds;
	it_val.it_value.tv_usec = ((int)(seconds*1000000))%1000000;
	it_val.it_interval.tv_sec = 0;
	it_val.it_interval.tv_usec = 0;

	if(setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		quit("could not set timer - setitimer() failed");
	}
}

void stopTimer() {
	struct itimerval it_val;

	it_val.it_value.tv_sec = 0;
	it_val.it_value.tv_usec = 0;
	it_val.it_interval.tv_sec = 0;
	it_val.it_interval.tv_usec = 0;
	
	if(setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		quit("could not stop timer - setitimer() failed");
	}
}

int extractMessageID(void* message) {
	return ntohl(*((uint32_t*) message));
}

int extractNumMessages(void* message) {
	return ntohl(*(((uint32_t*) message)+1));
}

int extractSequenceNum(void* message) {
	return ntohl(*(((uint32_t*) message)+2));
}

void* recvMessage(int ID, int* messageLength) {
	void* message = malloc(RESPONSE_MESSAGE_SIZE);
	while(true) {
		int len = recv(sock, message, RESPONSE_MESSAGE_SIZE, 0);
		if(len <= 0)
			quit("server doesn't exist or recv() failed");
		if(len < 12)
			quit("improper server response message -- doesn't include required headers");
		
		//accept message if it matches request ID
		if(extractMessageID(message) == ID) {
			*messageLength = len;
			return message;
		}
		plog("Received invalid response");
	}
}