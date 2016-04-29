#include "clientMessenger.h"
#include "utility.h"
#include "setupClientSocket.inc"
#include "Compression.h"

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
#include <stdbool.h>

const int RESPONSE_MESSAGE_SIZE = 1400;
const int COMMAND_MESSAGE_SIZE = 1400;

//UDP socket
int sock = -1;
char* robotID;

/* File nums */
int imageCount = 0;
int GPSCount = 0;
int DGPSCount = 0;
int lasersCount = 0;

typedef struct commandNode{
    struct commandNode *next;
    int index;
    char *command;
    bool received;
    bool shouldReceive;
} commandNode;

typedef struct commandResponse{
    uint32_t numReceived;
    uint32_t numMessages;
    uint32_t lastMessageLength;
    void **messages;
} commandResponse;

//private functions
void setTimer(double seconds);
void stopTimer();
void timedOut(int ignored);
commandNode *buildCommandList(char* requestString);
void freeCommandList(commandNode *commandList);
bool allDataReceived(commandNode *commandList);
int getCommandListLen(commandNode *commandList);
void setCommandReceived(commandNode *commandList, int commandNum);

void* recvMessage(int ID, int* messageLength);
void recvData(int ID, commandNode *commandList, double timeout);
void storeData(commandResponse response, char *command);
uint32_t extractMessageID(void* message);
uint32_t extractNumMessages(void* message);
uint32_t extractSequenceNum(void* message);
uint32_t extractCommandIndex(void* message);

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
	Send request
*/
void sendRequest(char* requestString, int* responseLength, double timeout) {
	static uint32_t ID = 0;
    int requestLen, 
    	metadataLen, 
    	numPackets, 
    	segmentLen;
    void *request;
    char *segment;

    //copy requestString for furure use
    char *requestStringCopy = requestString;

    // 12 bytes for ID, message count, and message index + robotID length 
    // + 1 byte for null char terminator for robotID
    metadataLen = 12+strlen(robotID)+1;
    
    // number of packets needed
    numPackets = ((strlen(requestString) + 1) / (COMMAND_MESSAGE_SIZE - metadataLen)) + 1;

	// send messages    
    int i;
    for(i = 0; i < numPackets; i++){
        if(strlen(requestString) + 1 > (COMMAND_MESSAGE_SIZE - metadataLen)) 
            segmentLen = COMMAND_MESSAGE_SIZE - metadataLen;
        else segmentLen = strlen(requestString) + 1;

        requestLen = segmentLen + metadataLen;
        request = malloc(requestLen);

        //insert ID
        *((uint32_t*)request) = htonl(ID);

        //insert number of messages
		*(((uint32_t*)request) + 1) = htonl(numPackets);

		//insert message index 
		*(((uint32_t*)request) + 2) = htonl(i);

		//insert robotID string
		memcpy(((char*)request)+12, robotID, strlen(robotID)+1);

		//insert command message segement
		segment = malloc(segmentLen);
		memcpy(segment, requestString, segmentLen);
		memcpy(((char*)request)+12+strlen(robotID)+1, segment, segmentLen);

		//Testing
		int byte;
		for(byte = 0; byte < requestLen; byte++){
			fprintf(stdout, "  %02x", *(((char*)request) + byte));
            if(((byte + 1 ) % 16) == 0)
                fprintf(stdout, "\n");
		}
		fprintf(stdout, "\n\n");
/*		for(byte = 0; byte < requestLen; byte++){
			fprintf(stdout, "%c", *(((char*)request) + byte));
		}
		fflush(stdout);
		plog("\nRequest length: %d\n", requestLen);
*/
		//send request
		int numBytesSent = send(sock, request, requestLen, 0);
		if(numBytesSent < 0)
			quit("could not send request - send() failed");
		else if(numBytesSent != requestLen)
			quit("send() didn't send the whole request");
	
		requestString += segmentLen;
		free(request);
		free(segment);
    }

    commandNode *commandList = buildCommandList(requestStringCopy);
    recvData(ID, commandList, timeout);
}

void recvData(int ID, commandNode *commandList, double timeout) {
    
    uint32_t numCommands = getCommandListLen(commandList);
    commandResponse *responses = malloc(sizeof(commandResponse) * numCommands);
    memset(responses, 0x00, sizeof(commandResponse) * numCommands);

    while(!allDataReceived(commandList)) {
	    setTimer(timeout);

	    /* Get the current message */
	    int messageLength;
	    void* message = recvMessage(ID, &messageLength);
	    uint32_t numMessages = extractNumMessages(message);
	    uint32_t seqNum = extractSequenceNum(message);
        uint32_t commandIndex = extractCommandIndex(message);
        
        /* Log tracking info */
/*      plog("ID: %d", extractMessageID(message));
	    plog("Total: %d", numMessages);
	    plog("Sequence: %d", seqNum);
        plog("Command Index: %d", commandIndex);
	    plog("Length: %d", messageLength);
*/
        /* Check if this is a valid command index */
        if(commandIndex >= numCommands)
            quit("Unexpected command index received in recvData().");

        /* Initialize the message's entry if it has not yet been done */
        if(responses[commandIndex].messages == NULL) {
            responses[commandIndex].messages = malloc(sizeof(void*)*numMessages);
            memset(responses[commandIndex].messages, 0x00, sizeof(void*)*numMessages);
            responses[commandIndex].numMessages = numMessages;
            responses[commandIndex].numReceived = 0;
	        responses[commandIndex].lastMessageLength = (numMessages == 1 ?
                                                          messageLength : 0);
        }
	    
        /* Check if this is a valid sequence number */
        if(seqNum >= numMessages)
            quit("Unexpected sequence number received in recvData().");

        /* Check if this message was a repeated receive */
        if(((char*)responses[commandIndex].messages[seqNum]) == NULL) {
            (responses[commandIndex].messages)[seqNum] = message;
            (responses[commandIndex].numReceived)++;

            /* If this is the last message for a response, record length */
            if(seqNum == numMessages - 1)
                responses[commandIndex].lastMessageLength = messageLength;
        } else {
            plog("Duplicate message received.");
            free(message);
        }

        /* Update if this command is now fully received */
        if(responses[commandIndex].numReceived ==
           responses[commandIndex].numMessages)
            setCommandReceived(commandList, commandIndex);
            fprintf(stderr, "Received message for command %d.\n", commandIndex);

        //stop timer
	    stopTimer();
    }

    /* Print the received command information */
    commandNode *currNode = commandList->next;
    int i = 0;
    while(currNode != NULL)
    {
        if(currNode->shouldReceive)
            storeData(responses[i], currNode->command);
        currNode = currNode->next;
        i++;
    }

    /* Update ID for next call */
    ID++;
    free(responses);
}

/* Prints the data in this commandReponse to a file based off the command type
 * in command. Destroys message data in commandResponse. */
void storeData(commandResponse response, char *command)
{
    /* Extract the full message from the commandResponse struct */
    int PAYLOAD_SIZE = RESPONSE_MESSAGE_SIZE - 16;
    uint32_t messageLen = ((response.numMessages - 1) * PAYLOAD_SIZE) +
                          (response.lastMessageLength - 16);
    void *fullResponse = malloc(messageLen);
    int i;

    /* Print tracking data */
  //  plog("Full response length: %d", messageLen);

    /* Loop through the received messages to piece them together */
    for(i = 0; i < response.numMessages - 1; i++) {
        memcpy(((char *)fullResponse) + (i * PAYLOAD_SIZE),
               ((char *)response.messages[i]) + 16, PAYLOAD_SIZE);
        free(response.messages[i]);
    }
    memcpy(((char *)fullResponse) + (i * PAYLOAD_SIZE),
           ((char *)response.messages[i]) + 16, response.lastMessageLength - 16);
    free(response.messages[response.numMessages - 1]);

    /* Open file for printing */
    FILE *file;
    char *fileName = (char *)malloc(50);
    memset(fileName, 0x00, 50);

    /* Create file name based on command type */
    if(strstr(command, "GET IMAGE") != NULL) {
        sprintf(fileName, "image-%d.jpg", imageCount++);
        uint32_t len = messageLen;
        uint32_t newLen = 0;
        char *comp = charDecompress_C((char *) fullResponse, len, &newLen);
        if(newLen != 0) {
            free(fullResponse);
            fullResponse = comp;
            messageLen = newLen;
        } else {
            plog("Invalid decompress in storeData().");
        }
    }
    else if(strstr(command, "GET DGPS") != NULL)
        sprintf(fileName, "DGPS-%d.txt", DGPSCount++);
    else if(strstr(command, "GET GPS") != NULL)
        sprintf(fileName, "GPS-%d.txt", GPSCount++);
    else if(strstr(command, "GET LASERS") != NULL)
        sprintf(fileName, "lasers-%d.txt", lasersCount++);
    else
    {
        plog("Command type not resolved in storeData().");
        return;
    }

    /* Open the file */
    file = fopen(fileName, "w+");
    if(fwrite(fullResponse, sizeof(char), messageLen, file) != messageLen)
        quit("fwrite() failed in storeData().");
    fclose(file);
    free(fullResponse);
}

commandNode *buildCommandList(char *responseString){
    int commandLen;
    int currIndex = 0;
    char* command;
    commandNode *head = malloc(sizeof(commandNode));
    memset(head, 0x00, sizeof(commandNode));
    commandNode *current = head;
    commandNode *next = NULL;

    while((commandLen = (char)(*responseString)) != 0){
        //extract next command
        command = malloc(50);
        memset(command, 0x00, 50);
        memcpy(command, responseString + 1, commandLen);
        
        //make new commandNode
        next = malloc(sizeof(commandNode));

        //fill fields of new commandNode
        next->index = currIndex++;
        next->command = command;
        next->received = false;
        next->next = NULL;
        if(strstr(command, "GET") != NULL) next->shouldReceive = true;
        else next->shouldReceive = false;

        // add commandNode to list
        current->next = next;
        current = next;
        next = NULL;

        //move responseString forward
        responseString += commandLen + 1;     
    }

    return head;
}

bool allDataReceived(commandNode *commandList){
    commandNode *current = commandList->next;

    while(current != NULL){
        if(current->shouldReceive && !current->received) return false;
        current = current->next;
    }
    return true;
}

void freeCommandList(commandNode *listHead){
    commandNode *current = listHead;
    commandNode *next = NULL;

    while(current != NULL){
        if(current->command != NULL) free(current->command);
        next = current->next;
        free(current);
        current = next;
    }
}

int getCommandListLen(commandNode *commandList)
{
    int len = 0;
    commandNode *current = commandList->next;

    while(current != NULL)
    {
        len++;
        current = current->next;
    }

    return len;
}

void setCommandReceived(commandNode *commandList, int commandNum)
{
    int currCommand = 0;
    commandNode *currNode = commandList->next;

    while((currCommand < commandNum) && (currNode != NULL))
    {
        currNode = currNode->next;
        currCommand++;
    }

    if(currNode == NULL)
        quit("Invalid commandNum in setCommandReceived().");

    currNode->received = true;
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

uint32_t extractMessageID(void* message) {
	return ntohl(*((uint32_t*) message));
}

uint32_t extractNumMessages(void* message) {
	return ntohl(*(((uint32_t*) message)+1));
}

uint32_t extractSequenceNum(void* message) {
	return ntohl(*(((uint32_t*) message)+2));
}

uint32_t extractCommandIndex(void* message) {
    return ntohl(*(((uint32_t*) message)+3));
}

void* recvMessage(int ID, int* messageLength) {
	void* message = malloc(RESPONSE_MESSAGE_SIZE);
	while(true) {
		int len = recv(sock, message, RESPONSE_MESSAGE_SIZE, 0);
		if(len <= 0)
			quit("server doesn't exist or recv() failed");
		if(len < 16)
			quit("improper server response message -- doesn't include required headers");
		
		//accept message if it matches request ID
		if(extractMessageID(message) == ID) {
			*messageLength = len;
			return message;
		}
		plog("Received invalid response");
	}
}
