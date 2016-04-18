/*  Name:       Matthew Pfister, Kelly Martin, Zack Derose
    Due Date:   24 April 2015 @ 2:30 PM
	Class:      CPSC 3600 - Section 1000
	Assignment: Group Project
*/

/* Brief Overview:
   Source file for handling a client's response - includes converting the response
   to a valid URI and HTTP GET request, sending the request to the robot, and forwarding
   the robot's response back to the client.
*/

#include "Robot.h"

#define MESSAGESIZE 1000   /* Size of receive buffer */

// ~~~ Function Prototypes
void DieWithError(char *errorMessage);  /* Error handling function */
void handleClient();
void convertCommand(char* recvRobotCommand, char** sendRobotCommand, char** sendRobotIP); 
void closeServer();

// ~~~ Command-line Parameters
unsigned short localPort; // local port
char* robotIP;            // Server IP address (dotted quad)
char* robotID;            // Robot ID
unsigned int imageID;     // Image ID

unsigned int recvcommID;

// ~~~ TCP Variables
int sockToRobot;                  // Socket descriptor
struct sockaddr_in robotServAddr; // robot server address
struct hostent *thehost;          // Hostent from gethostbyname()

unsigned int getRequestLen;    // Length of string to robot
int bytesRcvd, totalBytesRcvd; // Bytes read in single recv() and total bytes read
char recvRobotID[4096];

// ~~~ UDP Variables
int sockToClient;                  // Socket
struct sockaddr_in middlewareAddr; // Local address
struct sockaddr_in robotClntAddr;  // Client address
unsigned int cliAddrLen;           // Length of incoming message
unsigned short robotServPort;      // Server port
int recvMsgSize;                   // Size of received message

char UDPRecvBody[MESSAGESIZE];
char TCPRecvBody[1000000];
unsigned int returnMessageSize;

int main(int argc, char *argv[]) {
    // ~~~ Redirect control interruptions
    signal(SIGINT, closeServer);
	
    // ~~~ Check for proper usage
    if (argc != 5) {
       fprintf(stderr, "Usage: robotServer server_port IP/host_name robot_ID image_ID\n");
       exit(1);
    }

    // ~~~ Parse the command-line parameters
    localPort     = atoi(argv[1]);
    robotIP       = argv[2];
    robotID       = argv[3];
    imageID       = atoi(argv[4]);

    // ~~~ Intelligently parse out an overly complex URL
    char* parser;
    if((parser = strstr(robotIP, "://"))) robotIP = parser + 3;
    if((parser = strchr(robotIP, '/'))) *parser = '\0';

    // ~~~~~ UDP SETUP

    // ~~~ Create socket for sending/receiving datagrams
    if ((sockToClient = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) DieWithError("\nsocket() failed\n");

    // ~~~ Construct local address structure
    memset(&middlewareAddr, 0, sizeof(middlewareAddr)); // Zero out structure
    middlewareAddr.sin_family = AF_INET;                // Internet address family
    middlewareAddr.sin_addr.s_addr = htonl(INADDR_ANY); // Any incoming interface
    middlewareAddr.sin_port = htons(localPort);         // Local port

    // ~~~ Bind to the local address
    printf("Middleware: About to bind to port %d\n", localPort);    
    if (bind(sockToClient, (struct sockaddr *) &middlewareAddr, sizeof(middlewareAddr)) < 0) DieWithError("\nbind() failed\n");

    while(1) {
        // ~~~ Set the size of the in-out parameter
        cliAddrLen = sizeof(robotClntAddr);

        // ~~~ Block until receive message from a client
        if ((recvMsgSize = recvfrom(sockToClient, UDPRecvBody, MESSAGESIZE, 0, (struct sockaddr *) &robotClntAddr, &cliAddrLen)) < 0)
            DieWithError("\nrecvfrom() failed\n");

		// ~~~ Check that the received message is of an appropriate size
        if(recvMsgSize > MESSAGESIZE) DieWithError("\nReceived a message from a client that was too large.\n");

		// ~~~ Indicate what client we are handling
        printf("Handling client %s\n", inet_ntoa(robotClntAddr.sin_addr));

		// ~~~ Handle the client
        handleClient();
        if(strcmp(recvRobotID, robotID)) continue;

		// ~~~ Parse through the header data and get to the actual message body
        char* bodyPtr = strstr(TCPRecvBody, "\r\n\r\n") + 4;
        returnMessageSize -= (intptr_t)bodyPtr - (intptr_t)TCPRecvBody;

		// ~~~ Determine how many segments to send
        unsigned int segments = ceil(returnMessageSize * 1.0 / (MESSAGESIZE - 12));
        unsigned int remainder = returnMessageSize % (MESSAGESIZE - 12);

		// ~~~ Send each individual segment to the client over UDP
        int i;
        for(i = 0; i < segments; i++) {
            char* toSend = (char*)malloc(MESSAGESIZE);
            parser = toSend;
			
			// ~~~ Insert the header data into the message
            *(unsigned int*)parser = htonl(recvcommID); parser += 4; // Insert the commID
            *(unsigned int*)parser = htonl(segments);   parser += 4; // Insert the number of segments
            *(unsigned int*)parser = htonl(i);          parser += 4; // Insert the index

		    // ~~~ Send the appropriate size for the segment   
            if(i == segments - 1) {
                memcpy(parser, bodyPtr, remainder); // Pack the robot's response into the message
                sendto(sockToClient, toSend, remainder + 12, 0, (struct sockaddr *) &robotClntAddr, sizeof(robotClntAddr));
            }
            else {    
                memcpy(parser, bodyPtr, MESSAGESIZE - 12); // Pack the robot's response into the message
                sendto(sockToClient, toSend, MESSAGESIZE, 0, (struct sockaddr *) &robotClntAddr, sizeof(robotClntAddr));
            }

            bodyPtr += MESSAGESIZE - 12; // Move on to the next part of the message
        }
    }

    // ~~~ Never Reached
}

// ~~~ handleClient(): Parse a client's request and (if the robot's ID is valid) forwards the request to the robot
void handleClient() {

    // ~~~ Extract the communication ID
    void* parser = UDPRecvBody;
    recvcommID = ntohl(*(unsigned int*)parser); parser += 4;

	// ~~~ Extract the robot ID
    strcpy(recvRobotID, (char*)parser); parser += (strlen((char*)parser) + 1);

	// ~~~ Ensure that the robot ID is valid
    if(strcmp(recvRobotID, robotID)) return;

	// ~~~ Extract the robot command
    char recvRobotCommand[4096];
    strcpy(recvRobotCommand, (char*)parser);

	// ~~~ Convert the client's command into a valid URI
    char* sendRobotCommand = NULL;
	char* sendRobotIP = NULL;
    convertCommand(recvRobotCommand, &sendRobotCommand, &sendRobotIP);

	// ~~~ Construct the HTTP GET message
    char getRequest[10000];
    sprintf(getRequest, "GET %s HTTP/1.1\r\n", sendRobotCommand); free(sendRobotCommand);
    strcat(getRequest, "Host: ");
    strcat(getRequest, sendRobotIP); free(sendRobotIP);
    strcat(getRequest, "Connection: close\r\n\r\n");

    // ~~~~~ TCP SETUP

    // ~~~ Create a reliable, stream socket using TCP
    if ((sockToRobot = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) DieWithError("\nsocket() failed\n");

    // ~~~ Construct the server address structure
    memset(&robotServAddr, 0, sizeof(robotServAddr));     // Zero out structure
    robotServAddr.sin_family      = AF_INET;              // Internet address family
    robotServAddr.sin_addr.s_addr = inet_addr(robotIP);   // Server IP address
    robotServAddr.sin_port        = htons(robotServPort); // Server port

    // ~~~ If user gave a dotted decimal address, we need to resolve it
    if (robotServAddr.sin_addr.s_addr == -1) {
        thehost = gethostbyname(robotIP);
        robotServAddr.sin_addr.s_addr = *((unsigned long *) thehost->h_addr_list[0]);
    }

    /* Establish the connection to the robot server */
    if (connect(sockToRobot, (struct sockaddr *) &robotServAddr, sizeof(robotServAddr)) < 0) DieWithError("\nconnect() failed");
	
    getRequestLen = strlen(getRequest); // Determine input length

    // ~~~ Send the string to the server
    if (send(sockToRobot, getRequest, getRequestLen, 0) != getRequestLen) DieWithError("\nsend() sent a different number of bytes than expected\n");
	
	returnMessageSize = 0; // Set the size of the message to send back to the client to 0
	parser = TCPRecvBody;  // Set the parser to the front of the receive buffer
	
	// ~~~ Handle the case of receiving multiple TCP responses
	if(!strcmp(recvRobotCommand, "GET IMAGE"))
	do {
        if ((bytesRcvd = recv(sockToRobot, parser, 1000000, 0)) < 0) DieWithError("\nrecv() failed or connection closed prematurely\n");
		parser += bytesRcvd;            // Advance the parser
		returnMessageSize += bytesRcvd; // Increment the number of bytes to send back to the client
	} while(bytesRcvd != 0);
	
	// ~~~ Handle the case of receiving a single TCP response
	else if ((returnMessageSize = recv(sockToRobot, TCPRecvBody, 1000000, 0)) < 0) DieWithError("\nrecv() failed or connection closed prematurely\n");

    close(sockToRobot); // Close socket
}

// ~~~ convertCommand(): Converts a command from a client to an actual URI that the robot understands
void convertCommand(char* recvRobotCommand, char** sendRobotCommand, char** sendRobotIP) {
	// ~~~ Allocate sufficient memory for the robot IP and command
    *sendRobotIP      = (char*)malloc(strlen(robotIP) + 6);
    *sendRobotCommand = (char*)malloc(1000);

	// ~~~ Convert to the proper URI formatt...
    char* parser;
	// For GET IMAGE...
    if(strstr(recvRobotCommand, "GET IMAGE")) {
        sprintf(*sendRobotCommand, "%s%hu%s", "/snapshot?topic=/robot_", imageID, "/image&width=600&height=500");
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8081");
		  robotServPort = 8081;
    }
	// For GET GPS...
    else if(strstr(recvRobotCommand, "GET GPS")) {
        sprintf(*sendRobotCommand, "%s%s",    "/state?id=", robotID);
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8082");
		  robotServPort = 8082;
    }
	// For GET DGPS...
    else if(strstr(recvRobotCommand, "GET DGPS")) {
        sprintf(*sendRobotCommand, "%s%s",    "/state?id=", robotID);
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8084");
		  robotServPort = 8084;
    }
    // For GET LASERS...
    else if(strstr(recvRobotCommand, "GET LASERS")) {
        sprintf(*sendRobotCommand, "%s%s",    "/state?id=", robotID);
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8083");
		  robotServPort = 8083;
    }
	// For MOVE...
    else if((parser = strstr(recvRobotCommand, "MOVE"))) {
        float movementSpeed = atof(parser += 5);
        sprintf(*sendRobotCommand, "%s%s%s%f", "/twist?id=", robotID, "&lx=", movementSpeed);
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8082");
		  robotServPort = 8082;
    }
	// For TURN...
    else if((parser = strstr(recvRobotCommand, "TURN"))) {
        float movementSpeed = atof(parser += 5);
        sprintf(*sendRobotCommand, "%s%s%s%f", "/twist?id=", robotID, "&az=", movementSpeed);
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8082");
		  robotServPort = 8082;
    }
	// For STOP...
    else if(strstr(recvRobotCommand, "STOP")) {
        sprintf(*sendRobotCommand, "%s%s%s", "/twist?id=", robotID, "&lx=0&az=0");
        sprintf(*sendRobotIP, "%s%s", robotIP, ":8082");
		  robotServPort = 8082;
    }
	else DieWithError("\nCommand not recognized.\n");

    return;
}

// ~~~ DieWithError(): a simple function for dying with an error.
void DieWithError(char* errorMessage) {
    perror(errorMessage);
	 exit(1);
}

// ~~~ closeServer(): Shutdown the server upon receiving a control signal
void closeServer() {
   // Print the count of guesses and correct answers
   fprintf(stderr, "Closing server.\n");

   // ~~~ Close all of the ports
   close(sockToRobot);
   close(sockToClient);

   exit(1);
}
