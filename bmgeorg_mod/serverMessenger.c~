#include "serverMessenger.h"
#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>	//for uint32_t
#include <string.h>     //for memset()
#include <sys/socket.h> //for socket(), connect(), sendto(), and recvfrom()
#include <unistd.h>     //for close()
#include <netdb.h>		//for struct sockaddr*

const int RESPONSE_MESSAGE_SIZE = 1000;

void setID(void* message, int ID);
void setNumMessages(void* message, int numMessages);
void setSequenceNum(void* message, int sequenceNum);

void sendResponse(int sock, struct sockaddr_in* recipientAddr, int addressSize, int ID, void* response, int responseLength) {
	int PAYLOAD_SIZE = RESPONSE_MESSAGE_SIZE - 12; //subtract size for headers
	int numMessages = responseLength/PAYLOAD_SIZE + (responseLength%PAYLOAD_SIZE == 0? 0 : 1);
	if(responseLength == 0)
		numMessages = 1;

	void* messages[numMessages];
	
	//allocate space for messages
	int i;
	for(i = 0; i < numMessages; i++) {
		int size = RESPONSE_MESSAGE_SIZE;
		if(i == numMessages-1 && (responseLength%PAYLOAD_SIZE != 0 || responseLength == 0))
			size = 12+responseLength%PAYLOAD_SIZE;
		messages[i] = malloc(size);
	}
	
	//set data
	for(i = 0; i < numMessages; i++) {
		setID(messages[i], ID);
		setNumMessages(messages[i], numMessages);
		setSequenceNum(messages[i], i);
		int size = PAYLOAD_SIZE;
		//only last message has a different size
		if(i == numMessages-1 && (responseLength%PAYLOAD_SIZE != 0 || responseLength == 0))
			size = responseLength%PAYLOAD_SIZE;
		memcpy(((char*)messages[i])+12, ((char*)response)+i*PAYLOAD_SIZE, size);
	}
	
	//send all messages
	for(i = 0; i < numMessages; i++) {
		int size = RESPONSE_MESSAGE_SIZE;
		if(i == numMessages-1 && (responseLength%PAYLOAD_SIZE != 0 || responseLength == 0))
			size = 12+responseLength%PAYLOAD_SIZE;
		int numSent = sendto(sock, messages[i], size, 0, (struct sockaddr*) recipientAddr, addressSize);
		if(numSent < 0)
			quit("sendto() failed in serverMessenger");
		else if(numSent != size)
			quit("sendto() did not send full message in serverMessenger");
			
		plog("Sent UDP message %d with %d bytes\n", i, numSent);
	}
	
	//free all messages
	for(i = 0; i < numMessages; i++) {
		free(messages[i]);
	}
}

void setID(void* message, int ID) {
	*((uint32_t*) message) = htonl(ID);
}

void setNumMessages(void* message, int numMessages) {
	*(((uint32_t*) message)+1) = htonl(numMessages);
}

void setSequenceNum(void* message, int sequenceNum) {
	*(((uint32_t*) message)+2) = htonl(sequenceNum);
}