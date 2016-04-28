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

void sendTraceCommands(int numSides, int sideLen, bool clockwise);
void recieveTraceCommands(int numSides);
char *addSnapshot(char* buffer);
double getTime();

int L; //L >= 1
int N; //4 <= N <= 8
int fileCount = 0;

const double COMMAND_TIMEOUT = 0.95;
const double DATA_TIMEOUT = 5.0;

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

   plog("Command String length: %d\n", (int)strlen(commandBuffer));
   plog("Command String: \n%s\n", commandBuffer);

   sendRequest(commandBuffer, &dummy, 10.0);
}

char* addSnapshot(char* buffer){
   //add commands in format [command len][command]
   char *command = (char*)malloc(15);

   sprintf(command, "%cGET IMAGE", (char)9);
   buffer = strcat(buffer, command);
   
   sprintf(command, "%cGET DGPS", (char)8);
   buffer = strcat(buffer, command);

   sprintf(command, "%cGET GPS", (char)7);
   buffer = strcat(buffer, command);

   sprintf(command, "%cGET LASERS", (char)10);
   buffer = strcat(buffer, command);

   free(command);
   return buffer;
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
    sendTraceCommands(N-1, L, false);	

    return 0;
}
