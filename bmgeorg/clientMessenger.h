#ifndef CLIENT_MESSENGER_H
#define CLIENT_MESSENGER_H

/*
	clientMessenger handles all communication with the server. All networking/socket code
	belongs in this file, along with all code for reconstructing the server's response.
*/

extern const int RESPONSE_MESSAGE_SIZE;

//public functions
void setupMessenger(char* serverHost, char* serverPort, char* robotID);

/*
	Send request, receive and reassemble response
	Exit if you don't receive entire response after timeout seconds
	Set responseLength to total length of response
	Return pointer to response data
*/
void* sendRequest(char* requestString, int* responseLength, double timeout);

#endif