/*  Name:       Matthew Pfister, Kelly Martin, Zack Derose
    Due Date:   24 April 2015 @ 2:30 PM
	Class:      CPSC 3600 - Section 1000
	Assignment: Group Project
*/

/* Brief Overview:
   Source file for handling sending robot commands necessary to complete both of
   the polygon traces and archive the robot's response as they arrive.
*/

#include "Robot.h"

#define TIMEOUT 5.0
#define MESSAGESIZE 1000

// ~~~ Function Prototypes
void DieWithError(char *errorMessage);
void clientInit(unsigned int polygonSides);
double getTime();
double getNanoTime(double time);
void runCommands(unsigned int polygonSides, unsigned int numCommands);

// ~~~ Global Variables
// Command-line Parameters
char* servIP;                 // IP address of server
unsigned short robotServPort; // robot server port
char* robotID;                // Robot ID
unsigned int L;               // The length of each side of the polygons
unsigned int N;               // The number of sides for each polygon

// Robot Movement and Action Parameters
char* commands[8];
double waitTimes[8];

float linearVelocity;
float movementDelay;

float angularVelocity;
float turnDelay;

// Client-to-Server Parameters
int sock;                         // Socket descriptor
struct sockaddr_in robotServAddr; // robot server address
struct sockaddr_in fromAddr;      // Source address of robot
struct hostent *thehost;          // Hostent from gethostbyname()

unsigned int fromSize;            // In-out of address size for recvfrom()

char recvBody[MESSAGESIZE];
int respStringLen;                // Length of received response

fd_set readfds; // The set
struct timeval tv; // To hold the time we wish to wait on sending

// ~~~ Random Communication ID
unsigned int commID;

int main(int argc, char *argv[]) {
    // ~~~ Set the randomized communication ID;
    srand(time(NULL));
    commID = (unsigned int)rand();

    // ~~~ Test for proper command-line usage
    if (argc != 6) DieWithError("\nUsage: ./robotClient IP/host_name port ID L N\n");
       
    // ~~~ Parse the command-line parameters
    servIP        = argv[1];       // First  arg: server IP address (dotted quad)
    robotServPort = atoi(argv[2]); // Second arg: server port
    robotID       = argv[3];       // Third  arg: the robot's ID
    L             = atoi(argv[4]); // Fourth arg: the length of each side of the polygons
    N             = atoi(argv[5]); // Fifth  arg: the number of sides of each polgyon
    
    // ~~~ Check for the proper number of sides
    if(N > 8) DieWithError("\nN was too large (greater than 8)\n");
    if(N < 4) DieWithError("\nN was too small (less than 4)\n");

    // ~~~ Intelligently parse out an overly complex URL
    char* parser;
    if((parser = strstr(servIP, "://"))) servIP = parser + 3;
    if((parser = strchr(servIP, '/'))) *parser = '\0';

    // ~~~ Create a datagram/UDP socket
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) DieWithError("\nsocket() failed\n");

    // ~~~ Construct the server address structure
    memset(&robotServAddr, 0, sizeof(robotServAddr));  // Zero out structure
    robotServAddr.sin_family = AF_INET;                // Internet addr family
    robotServAddr.sin_addr.s_addr = inet_addr(servIP); // Server IP address
    robotServAddr.sin_port   = htons(robotServPort);   // Server port

    // ~~~ If user gave a dotted decimal address, we need to resolve it
    if (robotServAddr.sin_addr.s_addr == -1) {
        thehost = gethostbyname(servIP);
        robotServAddr.sin_addr.s_addr = *((unsigned long *) thehost->h_addr_list[0]);
    }

    // ~~~ Set the socket as non-blocking
    fcntl(sock, F_SETFL, O_NONBLOCK); // Set the socket to no longer block

    // ~~~ Draw the first shape (of N sides)
    clientInit(N);
    runCommands(N, 8);

    // ~~~ Draw the second shape (of N sides)
    clientInit(N - 1);
    runCommands(N - 1, 8);
	
	// ~~~ Gather data about the robot's final position
	runCommands(1, 4);

    return 0;
}

void DieWithError(char *errorMessage){
    perror(errorMessage);
    exit(1);
}

// ~~~ clientInit(): prepares the waitTimes and commands arrays with the
//                   appropriate delay and command combinations
void clientInit(unsigned int polygonSides) {
    // ~~~ Prepare the data commands (first 4 commands)
    commands[0] = "GET IMAGE";  waitTimes[0] = 0;
    commands[1] = "GET GPS";    waitTimes[1] = 0;
    commands[2] = "GET DGPS";   waitTimes[2] = 0;
    commands[3] = "GET LASERS"; waitTimes[3] = 0;

    // ~~~ Prepare the velocities and delays
    // Prepare movement variables
    if(L < 6) {
        linearVelocity = L * 1.0 / 6;
        movementDelay  = 6;
    }
    else {
        linearVelocity = 1;
        movementDelay  = L;
    }

    // Prepare turn variables
    turnDelay    = 6;
    angularVelocity = (M_PI -(M_PI * (polygonSides - 2) / polygonSides)) / turnDelay;

    // ~~~ Prepare the movement commands
    int i;
    for(i = 4; i < 8; i++) commands[i] = (char*)malloc(100);
    sprintf(commands[4], "MOVE %f", linearVelocity);  waitTimes[4] = movementDelay;
    sprintf(commands[5], "STOP");                     waitTimes[5] = 0;
    sprintf(commands[6], "TURN %f", angularVelocity); waitTimes[6] = turnDelay;
    sprintf(commands[7], "STOP");                     waitTimes[7] = 0;
}

// ~~~ runCommands(): Runs the necessary commands for the robot to traverse all sides
//                    of a single polygon, periodically collecting data about its travel.
void runCommands(unsigned int polygonSides, unsigned int numCommands) {
    // ~~~ Save the data to an appropriately named file
    char* filename = (char*)malloc(4096); // Allocate memory for the filename
    if(numCommands == 8) sprintf(filename, "Polygon of %hu Sides.data", polygonSides); // Prepare the filename
	else                 sprintf(filename, "Final State.data");
    FILE* fp = fopen(filename, "w"); // Open up the file for writing
    
    // ~~~ Run all 8 of the commands
    unsigned int index;
    
    int i, n;
    for(index = 0; index < polygonSides; index++) { 
		if(numCommands == 8) fprintf(stderr, "~~~ Sending commands for %hu-sided Polygon, side %hu\n", polygonSides, index);
		else                 fprintf(stderr, "~~~ Printing out the final state of the robot\n");
    for(n = 0; n < numCommands; n++) {
        // ~~~ Indicate which command we are running
        fprintf(stderr, "Issuing robot command: %s", commands[n]);        
        
        // ~~~ Build UDP Message Body
        // Allocate memory for the message body
        unsigned int messageSize = 6 + strlen(robotID) + strlen(commands[n]);
        void* messageBody = (void*)malloc(messageSize);

        // Insert the three necessary components
        void* parser = messageBody;
        *(unsigned int*)parser = htonl(commID); parser += 4;           // 1) Insert the randomized communication ID
        strcpy((char*)parser, robotID); parser += strlen(robotID) + 1; // 2) Insert the robot ID
        strcpy((char*)parser, commands[n]);                            // 3) Insert the command

        // ~~~ The message to the middleware
        if (sendto(sock, messageBody, messageSize, 0, (struct sockaddr *) &robotServAddr, sizeof(robotServAddr)) != messageSize)
            DieWithError("\nsendto() sent a different number of bytes than expected\n");

        free(messageBody); // Free the memory of the message, now that it is not longer in use

        // ~~~ Declare the variables involved in receiving the response        
        unsigned int finalMessageSize;  // The message size of the pesky last part of the message
        unsigned int robotResponseSize; // Size (number of characters) of the robot's response
        unsigned int recvCommID;        // The communication ID that we received back from the server
        unsigned int toRecv;            // The number of messages we expect to receive
        unsigned int recvIndex;         // The index of the message are are currently receiving
        char** messageSegments = NULL;  // The array of strings that will hold the constituent parts of the message

        double remainingTime     = TIMEOUT;   // The remaining time in seconds
        double remainingNanoTime = 0;         // The remaining time in nanoseconds
        double beginTime         = getTime(); // The time upon sending the message

        // ~~~ Continue until we break due to receive the entirety of the respone
        while(1) {
            // ~~~ Prepare the time-out structure
            FD_ZERO(&readfds);              // Remove everything from the set
            FD_SET(sock, &readfds);         // Add our socket to the set
            tv.tv_sec  = remainingTime;     // We shall wait for the remaining time (seconds part)
            tv.tv_usec = remainingNanoTime; // We shall wait for the remaining time (nanoseconds

            // ~~~ Wait to see if a message arrives in time
            if(select(sock + 1, &readfds, NULL, NULL, &tv)) {
                // ~~~ Receive the message
                fromSize = sizeof(fromAddr);
                respStringLen = recvfrom(sock, recvBody, MESSAGESIZE, 0, (struct sockaddr *) &fromAddr, &fromSize);

                // ~~~ Determine the important variables from the response            
                robotResponseSize = respStringLen - 12;

                // ~~~ Parse the relevant information from the middleware response
                parser = recvBody;
                recvCommID = ntohl(*(unsigned int*)parser); parser += 4; // Ascertain the received comm ID
                toRecv     = ntohl(*(unsigned int*)parser); parser += 4; // Ascertain the number of messages to receive
                recvIndex  = ntohl(*(unsigned int*)parser); parser += 4; // Ascertain the index of the message

                // ~~~ Determine if the message should be ignored
                if (robotServAddr.sin_addr.s_addr != fromAddr.sin_addr.s_addr) DieWithError("\nReceived a message from an unknown source.\n");
                if (commID != recvCommID) DieWithError("\nReceived a message from an unknown source.\n");

                // ~~~ Prepare the array which holds the message segments if needed
                int j;
                if(messageSegments == NULL) {
                    messageSegments = (char**)malloc(sizeof(char*) * toRecv); // Create the necessary memory
                    for(j = 0; j < toRecv; j++) messageSegments[j] = NULL;    // Set all of the pointers to NULL
                }
                
                // ~~~ Place the message in the table if it has not been already
                if(messageSegments[recvIndex] == NULL) {
                    messageSegments[recvIndex] = (char*)malloc(robotResponseSize);    // Make enough memory for the message
                    memcpy(messageSegments[recvIndex], parser, robotResponseSize);    // Copy the memory over
                    if(recvIndex == toRecv - 1) finalMessageSize = robotResponseSize; // And save the size if this is the last segment
                }

                // ~~~ Determine whether or not we have received all of the segments
                _Bool doBreak = 1;
                for(j = 0; j < toRecv; j++) if(messageSegments[j] == NULL) doBreak = 0;
                if(doBreak) break;
                
				// ~~~ Allow for any additional wait times if needed to get all segments
                double currentTime = getTime();               // Get the current time
                double elapsedTime = currentTime - beginTime; // Calculate how much time has elapsed
                remainingTime = TIMEOUT - elapsedTime;        // Calculate the remaining time we must wait
                if(remainingTime < 0) DieWithError("\nData not received within timeout delay.\n");
                else {
                    remainingNanoTime = getNanoTime(remainingTime);
                    remainingTime     = (int)remainingTime;
                }
            }
            else DieWithError("\nData not received within timeout delay.\n");
        }
        
        // ~~~ Determine if we are saving text data to the log or image data to a new file
        FILE* tempFP; // Temp file pointer
        if(n == 0) { // Save the image
            if(numCommands == 8) sprintf(filename, "Polygon %hu(%hu).jpg", polygonSides, index);
			else                 sprintf(filename, "Final Snapshot.jpg");
            tempFP = fopen(filename, "w");
        }
        else { // Log the data
            if(numCommands == 8) sprintf(filename, "Polygon %hu(%hu) - %s\n", polygonSides, index, commands[n]);
			else                 sprintf(filename, "%s\n", commands[n]);
            tempFP = fp;
            fputs(filename, tempFP); // Title this section of data
        }       
        
		if(n == 1 || n == 2) fprintf(stderr, ": ");
		
        int j, k;
        // ~~~ Save all of the message segments but the last
        for(j = 0; j < toRecv - 1; ++j)
        for(k = 0; k < MESSAGESIZE - 12; k++) {
			fputc(messageSegments[j][k], tempFP);
			if(n == 1 || n == 2) fprintf(stderr, "%c", messageSegments[j][k]);
		}
        
        // ~~~ Save the filename message segment
        for(i = 0; i < finalMessageSize; i++) {
			fputc(messageSegments[toRecv - 1][i], tempFP);
	        if(n == 1 || n == 2) fprintf(stderr, "%c", messageSegments[toRecv - 1][i]);
		}
		
	    fprintf(stderr, "\n");

        // ~~~ Free the array of segments
        for(i = 0; i < toRecv; i++) free(messageSegments[i]);
        free(messageSegments);
        
		// ~~~ Finish up either the log entry or image write
        if(n != 0) { fputs("\n\n", tempFP); fp = tempFP; }
        else       fclose(tempFP);
        
        ++commID; // Increment the commID

        // ~~~ Allow for more time to wait on the robot's movement
        double currentTime = getTime(); // Get the current time
        double sleepTime = waitTimes[n] - (currentTime - beginTime); // Calculate the remaining sleep time
        if(sleepTime > 0) { // If we need to sleep...
		    // ~~~ Calculate the components of the amount of time we should sleep
            double sleepNanoTime = getNanoTime(sleepTime); // In nanoseconds
            sleepTime = (int)sleepTime;                    // In seconds
            
			// ~~~ Formulate the timespec structure
            struct timespec ts;
            ts.tv_sec  = sleepTime;
            ts.tv_nsec = sleepNanoTime;
            nanosleep(&ts, NULL); // Sleep for the remaining duration
        }
		
		
    }
	    fprintf(stderr, "\n\n");
    }
	
    // ~~~ Memory Management
    fclose(fp);      // Close the file
    free(filename); // Free the filename
}

// ~~~ getTime(): gets the current time of the system in seconds
double getTime() {
        struct timeval curTime;
        (void) gettimeofday (&curTime, (struct timezone *) NULL);
        return (((((double) curTime.tv_sec) * 1000000.0) 
             + (double) curTime.tv_usec) / 1000000.0); 
}

// ~~~ getNanoTime(): get the nano seconds component of the specified time
double getNanoTime(double time) {
        double nanoTime = time - (int)time;
        nanoTime *= 1000000000;
        return (int)nanoTime;
}