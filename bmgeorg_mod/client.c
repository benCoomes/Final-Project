#include "clientMessenger.h"
#include "utility.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>

void tracePolygon(int numSides, int sideLen, bool clockwise);
void getSnapshot();
void sendTraceCommands(int numSides, int sideLen, bool clockwise);
void recieveTraceCommands(int numSides);
char *addSnapshot(char* buffer);
double getTime();

int L; //L >= 1
int N; //4 <= N <= 8
int fileCount = 0;

const double COMMAND_TIMEOUT = 0.95;
const double DATA_TIMEOUT = 5.0;

void tracePolygon(int numSides, int sideLen,  bool clockwise) {
/*~ need to build the supermessage body
    send it to the server
    recieve confirmation message
    recieve all other incoming data 

    perhaps this could be broken up like this:
    tracePolygon(...){
        sendTraceCommands(...){
            build commands
            send commands
            confirm successful arrival, or try again if failure
        }
        recieveTrace(...){
            recieve a message
            use header to decided how to process next message(s)
            perform file IO
            end when FINISHED protocol parameters for finishing are met (TDB) 
            or when timeout occurs (print error that all data may not be present)
        }
    }
*/
   int dummy;
   double timeSpent;
   double sleepTime;
   int waitSeconds;
   int waitUSeconds;
   
   plog("tracing polygon of order %d", numSides);
   plog("Clockwise: %d", clockwise);
   
   //Determine the angle the robot should turn.
   double turnAngle = M_PI - ((numSides - 2)*M_PI/numSides);
   
   plog("Turn angle: %lf", turnAngle);
   
   //Create a turn request for pi/4 radians per second.
   char *turnRequest = (char *)malloc(20);
   if(clockwise) sprintf(turnRequest, "TURN %.10f", -M_PI/4);
   else sprintf(turnRequest, "TURN %.10f", M_PI/4);
   
   plog("Turn request: %s", turnRequest);
   
   //Take initial screenshot before making the polygon.
   getSnapshot();

   //Logic for tracing the polygon.
   int i;
   for(i = 0; i < numSides; i++) {
      plog("Iteration: %d", i);
      
      plog("Sending MOVE 1 command");
      //Send a request to begin moving.
      timeSpent = getTime();
      sendRequest("MOVE 1", &dummy, COMMAND_TIMEOUT);
      timeSpent = getTime() - timeSpent;
      
      plog("Time spent sending request and getting response: %lf", timeSpent);

      //Calculate wait time (L - time spent in sendRequest).
      if(sideLen > timeSpent) {
         sleepTime = sideLen - timeSpent;
         
         plog("waiting for %lf seconds", sleepTime);
         
         waitSeconds = (int) sleepTime;
         sleepTime -= waitSeconds;
         waitUSeconds = (int) (sleepTime*1000000);

         //Wait until robot reaches destination.
         sleep(waitSeconds);
         usleep(waitUSeconds);
      }
      
      plog("sending stop command");
      
      //Send a request to stop the robot.
      sendRequest("STOP", &dummy, COMMAND_TIMEOUT);
      
      plog("sent stop command");

      //Take Snapshot after movement has ended.
      getSnapshot();
      
      plog("sending turnRequest: %s", turnRequest);
      //Send a request to begin turning.
      timeSpent = getTime();
      sendRequest(turnRequest, &dummy, COMMAND_TIMEOUT);
      timeSpent = getTime() - timeSpent;
      
      plog("Time spent sending request and getting response: %lf", timeSpent);
      
      /*
      	Manual tests show that the actual speed of the robot is about .89*requested speed
      	(with an absolute maximum speed of ~M_PI/4 radians/second)
      	This is an unavoidable hack since we are working with flawed hardware.
      */
      const double actualSpeed = .89*M_PI/4;
      //Calculate wait time (turnAngle/(M_PI/4) - time spent in sendRequest).
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
      
      plog("sending stop command");
      //Send a request to stop turning.
      sendRequest("STOP", &dummy, COMMAND_TIMEOUT);
      plog("sent stop command");
   }
}

void sendTraceCommands(int numSides, int sideLen, bool clockwise){
   int dummy;

   // commandBuffer has the following format: [command 0 length][command 0][command 1 length][command 1]...[0]
   char *commandBuffer = (char*)malloc(numSides * 120 + 40);
   char *command = (char*)malloc(50);
   char *copy = (char*)malloc(50);

   // determine the turn angle and speed
   double turnAngle = M_PI - ((numSides - 2)*M_PI/numSides);
   double turnSpeed;
   if(clockwise) turnSpeed = -M_PI/4;
   else turnSpeed = M_PI/4; 
   commandBuffer = addSnapshot(commandBuffer);

   int i;
   for(i = 0; i < numSides; i++){
      sprintf(command, "MOVE 1 %d", sideLen);
      strcpy(copy, command);
      sprintf(command, "%c%s", (int)strlen(copy), copy);       
      commandBuffer = strcat(commandBuffer, command);

      sprintf(command, "STOP");
      strcpy(copy, command);
      sprintf(command, "%c%s", (int)strlen(copy), copy);       
      commandBuffer = strcat(commandBuffer, command);

      commandBuffer = addSnapshot(commandBuffer);

      sprintf(command, "TURN %.10f %.10f", turnSpeed, turnAngle); 
      strcpy(copy, command);
      sprintf(command, "%c%s", (int)strlen(copy), copy);       
      commandBuffer = strcat(commandBuffer, command);

      sprintf(command, "STOP"); 
      strcpy(copy, command);
      sprintf(command, "%c%s", (int)strlen(copy), copy);       
      commandBuffer = strcat(commandBuffer, command);
   }

   // we need to make sure that the terminating null byte is sent over the network!
   // strcat takes care of appending the null byte to the send of command 

   plog("Command String length: %d\n", (int)strlen(commandBuffer));
   plog("Command String: \n%s\n", commandBuffer);

   sendRequest(commandBuffer, &dummy, 10.0);
}

char* addSnapshot(char* buffer){
   //add commands in format [command len][command]
   char *command = (char*)malloc(50);

   sprintf(command, "%cGET IMAGE", (char)9);
   buffer = strcat(buffer, command);
   
   sprintf(command, "%cGET DGPS", (char)8);
   buffer = strcat(buffer, command);

   sprintf(command, "%cGET GPS", (char)7);
   buffer = strcat(buffer, command);

   sprintf(command, "%cGET LASERS", (char)10);
   buffer = strcat(buffer, command);

   return buffer;
}

void getSnapshot() {
   /*
   int length;
   char *data;

   FILE *imageFile;       //File for the image data received.
   FILE *positionFile;    //File for the position data received.

   char *imageFileName = (char *)malloc(50);
   char *positionFileName = (char *)malloc(50);

   sprintf(imageFileName, "image-%d.jpg", fileCount);
   sprintf(positionFileName, "position-%d.txt", fileCount);
   ++fileCount;

   imageFile = fopen(imageFileName, "w+");
   positionFile = fopen(positionFileName, "w+");

   //Get the image and write the data to the image file created.
   data = (char *)sendRequest("GET IMAGE", &length, DATA_TIMEOUT);
   if(fwrite(data, 1, length, imageFile) != length) quit("fwrite failed");
   
   //The imageFile is no longer needed.
   free(data);
   fclose(imageFile);

   //Get GPS data from robot and print to positionFile
   data = (char *)sendRequest("GET GPS", &length, DATA_TIMEOUT);
   fprintf(positionFile, "GPS ");
   if(fwrite(data, 1, length, positionFile) != length) quit("fwrite failed");

   fprintf(positionFile, "\n");

   free(data);

   //Get DGPS data from robot and print to positionFile
   data = (char *)sendRequest("GET DGPS", &length, DATA_TIMEOUT);
   fprintf(positionFile, "DGPS ");
   if(fwrite(data, 1, length, positionFile) != length) quit("fwrite failed");

   fprintf(positionFile, "\n");

   //Get LASER data from robot and print to positionFile
   data = (char *)sendRequest("GET LASERS", &length, DATA_TIMEOUT);
   fprintf(positionFile, "LASERS ");
   if(fwrite(data, 1, length, positionFile) != length) quit("fwrite failed");
   
   fprintf(positionFile, "\n");

   free(data);
   fclose(positionFile);
   */
}

double getTime() {
   struct timeval curTime;
   (void) gettimeofday(&curTime, (struct timezone *)NULL);
   return (((((double) curTime.tv_sec) * 10000000.0)
      + (double) curTime.tv_usec) / 10000000.0);
}

int main(int argc, char** argv) {
	//get command line args
	if(argc != 6) {
		fprintf(stderr, "Usage: %s <server IP or server host name> <server port> <robot ID> <L> <N>\n", argv[0]);
		exit(1);	
	}
	char* serverHost = argv[1];
	char* serverPort = argv[2];
	char* robotID = argv[3];
	L = atoi(argv[4]);
	if(L < 1) {
		quit("L must be at least 1");
	}
	N = atoi(argv[5]);
	if(N < 4 || N > 8) {
		quit("N must be an integer the range [4, 8]");
	}
	
	plog("Read arguments");
	plog("Server host: %s", serverHost);
	plog("Server port: %s", serverPort);
	plog("Robot ID: %s", robotID);
	plog("L: %d", L);
	plog("N: %d", N);
	
	plog("Setting up clientMessenger");
	
	setupMessenger(serverHost, serverPort, robotID);

   sendTraceCommands(N, L, true);	
//	tracePolygon(N, L, true);
//	tracePolygon(N-1, L, false);

	return 0;
}
