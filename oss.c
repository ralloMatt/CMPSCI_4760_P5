#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

#include "header.h"

//Prototypes
void makeSharedMemory();//creates and initializes needed shared memory
void terminate(int sig);//terminates program, kills processes, and frees memory
void addToClock(int option);//adds to the clock depending on option
void processMessage(int action);//does what the message is asking to do
void releaseAllResources(int processNum); //releases all the resources when a process terminates
void releaseAResource(int processNum, int resourceNum); //release a specific resource
void deadlockAvoidance(int processNum, int resourceNum);
void printCurrentResources();

//Global Variables
int shmidClock; 
int shmidPCB;
Clock *systemClock; //shared clock
ProcessControlBlock *PCB; //shared PCB

ResourceDesc *resourceDesc; //shared resource
FILE *file; //so I can close file in terminate()
int totalRequestsGranted = 0;
int v = 0;

int main (int argc, char *argv[]) {
	
	printf("\nOSS C Program:\n\n");
		
	int option; //used to see if option is present
	int x = 18; //number of processes spawned (default 18)
	
	while ((option = getopt (argc, argv, "hs:v")) != -1){ //get option (user input)
		switch (option){
			case 'h': //help message
				printf("How to run: './oss -option'\n");
				printf("\tOption: '-h' ~ Displays options and what they do.\n");
				printf("\tOption: '-s x' ~ Determines how many 'x' processes that will be spawned.\n");
				printf("\tOption: '-v' ~ Turns Verbose on.\n");
				return 0;
				break;
			case 's':
				x = atoi(optarg);
				if(x == 0){ //if user put characters instead of number or put zero
					printf("Setting process to default 18 because either characters were given or argument was 0.\n\n");
					x = 18; //just set to default
				}
				if(x > 100){
					printf("The Max number of processes should be <= 100. Setting process count to 100.\n\n");
					x = 100; //set to 100
				}
				break;
			case 'v':
				printf("Verbose will be turned on.\n\n");
				v = 1;
				break;
			default: //invalid option
				return 0;
		}
	} //end while

	printf("%d processes will be spawned.\n", x);
	maxProcesses = x; //set maxProcesses
	
	if(v == 1){
		printf("Verbose is turned on.\n\n");
	}
	else{
		printf("Verbose is turned off.\n\n");
	}
	
	file = fopen("logFile.txt", "w"); //open file
	if(file == NULL){ //make sure file can be opened
		fprintf(stderr, "%s: ", argv[0]);
		perror("Error: \n");
		exit(1);
	}
	
	//Reset random number each time oss is ran
	time_t t;
	srand((int)time(&t) % getpid()); 

	makeSharedMemory(); //SHARED MEMORY
	createMessageQueue(); //MESAGE QUEUE

	resourceDesc = malloc(maxResources * sizeof(ResourceDesc));

	int i;
	for(i = 0; i < maxResources; i++){ //Initialize Resource Descriptors
		resourceDesc[i].instances = rand() % 10 + 1;
		resourceDesc[i].allocated = 0;
	}
	
	//SIGNAL HANDLERS
	signal(SIGALRM, terminate); //create signal handler
	signal(SIGINT, terminate); //terminate program if ctrl c is pressed
	alarm(2); //terminate program if two real seconds have passed
	
	//USER PROCESSES
	pid_t child_pid; //child process ID
	pid_t checkChild; //used to see if a child has ended
	int childStatus; //used to see status of child
	char arg[10]; //used to send in the process number
	int processesCompleted = 0;
	int processesRunning = 0;
	int processNum = 0;
	
	while(processesCompleted != maxProcesses){ //when all processes are completed end
		
		//Check for messages
		if(msgrcv(msgID, &msg, sizeof(msg), 1, IPC_NOWAIT) != -1){
			//if i get message do stuff
			processMessage(msg.whatIdo);
		} 
			
		if(processesRunning < 18 && processNum < maxProcesses){
			//don't run more than 18 proccesses at once	
			//and don't make more processes that was asked of
			addToClock(1); //adds 1 to 500 milliseconds to clock
			snprintf(arg,10,"%d",processNum); //send in processNum as argument

			child_pid = fork(); 
			if(child_pid == 0){ //in child
				execl("./user", "user", arg, NULL);
				// If we get here, exec failed
				fprintf(stderr,"%s failed to exec worker!\n\n",argv[0]);
				exit(0); //exit child
			}
			
			PCB[processNum].processNum = processNum; //put process number in PCB
			PCB[processNum].pid = child_pid; //put child process id in PCB	
			
			processNum++; //keep track of processes that were made
			processesRunning++;
		}//end if 
		
		for(i = 0; i < processNum; i++){ //check if any children has finished
			checkChild = waitpid(PCB[i].pid, &childStatus, WNOHANG);
			if(checkChild != 0 && checkChild != -1){ // 0 is alive, 1 is error
				//child terminated
				kill(PCB[i].pid, SIGKILL); //kill child process
				//printf("*OSS: Child Process %d has terminated.\n", i);
				processesCompleted++;
				processesRunning--;
				break; //get out of for loop!!!!
			}
		} //end for loop
				
		
		addToClock(0);//Add 1000 nanoseconds clock at each end of while loop
	
	}//end of main while loop
	
	wait(); //wait for children to end
	
	
	printf("OSS: System Clock = %d:%d\n", systemClock->seconds, systemClock->nanoseconds);
	printf("Total requests granted = %d\n", totalRequestsGranted);
	
	terminate(0); //clean up everything 
	return 0;
}

void makeSharedMemory(){//create shared memory that's needed
	//Get id to shared memory
	shmidClock = shmget( SHMKEY_CLK, BUFF_CLK_SZ, 0777 | IPC_CREAT); 
	shmidPCB = shmget( SHMKEY_PCB, BUFF_PCB_SZ * maxProcesses, 0777 | IPC_CREAT); 
	
	//check for errors
	if (shmidClock == -1 || shmidPCB == -1){ 
			perror("Error in shmget\n");
			exit (1);
	}
	
	//Get pointer to shared memory
	systemClock = shmat(shmidClock, 0, 0); 
	PCB = shmat(shmidPCB, 0, 0);
	
	//check for errors
	if((int *)systemClock == (int *) -1 || (int *)PCB == (int *) -1){ 
		perror("Error in shmat\n");
		exit(1);
	}
	
	//Initialize everything
	
	systemClock->seconds = 1; //offset from the beginning of a second
	systemClock->nanoseconds = 0;
	
	int i;
	int j;
	int maxClaimBound = 3; //change to influence the chance of resource denials
	
	for(i = 0; i < maxProcesses; i++){
		PCB[i].processNum = -1;
		PCB[i].pid = -1;
		PCB[i].blockedTimeSecs = 0;
		PCB[i].blockedTimeNano = 0;
		for(j = 0; j < maxResources; j++){
			PCB[i].maxClaims[j] = rand() % maxClaimBound + 1;
			PCB[i].resourcesClaimed[j] = 0;
		}		
	}
	
	return;
}

void terminate(int sig){ //Signal Handler
	if(sig == 14){ //SIGALRM sets sig to 14 when passed
		fprintf(stderr, "Two REAL seconds have passed. Terminating Program!\n");
	}
	if(sig == 2){ //SIGINT sets sig to 2 when passed
		fprintf(stderr, "\nYou've pressed ctrl c. Terminating Program!\n");
	}
	if(sig == 0){
		printf("\nProgram has ended sucessfully. Terminating Program!\n");
	}
	
	free(resourceDesc); //free memory that was malloced
	
	//Remove message queue
	if(msgctl(msgID, IPC_RMID, NULL) == -1){
      perror("msgctl");
      exit(1);
	}
	
	//detach memory
	shmdt(systemClock);
	shmdt(PCB);
	
	//deallocate shared memory and check for error
	if(	shmctl(shmidClock, IPC_RMID, NULL) == -1) 
		perror("Error in shmctl\n");
	if(	shmctl(shmidPCB, IPC_RMID, NULL) == -1) 
		perror("Error in shmctl\n");
		
	fclose(file); //close file
	
	exit(0); //terminate program
}

void addToClock(int option){ //Adds to clock each time function is called
	int addTime = 0;
	
	if(option == 0){//add normally
		addTime = 1000;
	}
	if(option == 1){//add 1 to 500 milliseconds to simulate forking of child randomly
		addTime = (rand() % 500 + 1) * 1000000;//1 milliseconds is 1 mil nanoseconds
	}
	if(option == 2){//indicate work was done
		addTime = rand() % 10000000;
	}
	systemClock->nanoseconds += addTime;
	if(systemClock->nanoseconds >= 1000000000){ 
		// 1 billion nanoseconds = 1 second
		systemClock->seconds += 1; //add one to seconds
		systemClock->nanoseconds -= 1000000000;
	}
	return;
}

void processMessage(int action){
	switch(action){
		case 0: //Process is terminating
			if(v == 1){
				fprintf(file, "*Master has detected Process P%d terminating at time %d:%d Releasing all of processes' resources.\n", msg.processNum, systemClock->seconds, systemClock->nanoseconds);
			}
			//Run code to release all resources
			releaseAllResources(msg.processNum);
			
			break;
		case 1: //Process wants to request a resource
			if(v == 1){
				fprintf(file, "Master has detected Process P%d requesting R%d at time %d:%d\n", msg.processNum, msg.resourceNum, systemClock->seconds, systemClock->nanoseconds);
			}
			//Run code for deadlock avoidance and maybe bankers' algorhithm
			
			deadlockAvoidance(msg.processNum, msg.resourceNum);
			
			//Should have sent message in deadlockAvoidance()
			
			break;
		case 2: //Process wants to release a resource
			if(v == 1){
				fprintf(file, "Master has acknowledged Process P%d releasing R%d at time %d:%d\n", msg.processNum, msg.resourceNum, systemClock->seconds, systemClock->nanoseconds);
			}
			//Run code to release a specific resource
			//NOTE: User will not ask to release a resource it does not have
			releaseAResource(msg.processNum, msg.resourceNum);
			
			//Send message back to let user know resource was released
			msg.mtype = 3;
			if(msgsnd(msgID, &msg, sizeof(msg), 0) == -1){
				perror("OSS: Failed to send message");
			}
			

			break;
		default:
			break;
	}
	
	return;
}

void releaseAllResources(int processNum){
	int i;
	for(i = 0; i < maxResources; i++){ //Index i is the specific resource
		resourceDesc[i].allocated = resourceDesc[i].allocated - PCB[processNum].resourcesClaimed[i]; 
		resourceDesc[i].instances = resourceDesc[i].instances + PCB[processNum].resourcesClaimed[i];
		PCB[processNum].resourcesClaimed[i] = 0;
	}
	
	addToClock(2);
	return;
}

void releaseAResource(int processNum, int resourceNum){
	PCB[processNum].resourcesClaimed[resourceNum]--; //release one resource
	resourceDesc[resourceNum].allocated--; //release one from allocated
	resourceDesc[resourceNum].instances++; //add one to available
	
	addToClock(2);
	return;
}

void deadlockAvoidance(int processNum, int resourceNum){	
	if(resourceDesc[resourceNum].instances > 0){ //means resource is available
		resourceDesc[resourceNum].instances--;
		resourceDesc[resourceNum].allocated++;
		PCB[processNum].resourcesClaimed[resourceNum]++;
		totalRequestsGranted++;
		
		if(totalRequestsGranted % 20 == 0 && v == 1){ //v == 1 means verbose is on 
			fprintf(file, "Master granting P%d request R%d at time %d:%d\n", processNum, resourceNum, systemClock->seconds, systemClock->nanoseconds);
			printCurrentResources();
		}
		
		msg.mtype = 4;
		if(msgsnd(msgID, &msg, sizeof(msg), 0) == -1){
			perror("OSS: Failed to send message");
		}
	}
	else{ //not available
		fprintf(file, "Master blocking P%d for requesting R%d at time %d:%d\n", processNum, resourceNum, systemClock->seconds, systemClock->nanoseconds);
		
		
		msg.mtype = 4;
		if(msgsnd(msgID, &msg, sizeof(msg), 0) == -1){
			perror("OSS: Failed to send message");
		}
	}
	
	addToClock(2);
	return;
}

void printCurrentResources(){
	
	int i;
	int j;
	fprintf(file, "Current System Resources\n\t");
	for(i = 0; i < maxResources; i++){
		fprintf(file, "R%d   ", i);
	}
	fprintf(file, "\n");
	for(i = 0; i < maxProcesses; i++){
		fprintf(file, "P%d\t", i);
		for(j = 0; j < maxResources; j++){
			fprintf(file," %d    ", PCB[i].resourcesClaimed[j]);
		}
		fprintf(file,"\n");
	}
	
	return;
}

