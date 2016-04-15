BINS=robotClient robotServer

all: $(BINS)

robotClient: RobotClient.c
	gcc -g -Wall RobotClient.c -o robotClient -lm

robotServer: RobotServer.c
	gcc -g -Wall RobotServer.c -o robotServer -lm

clean:
	rm $(BINS)
