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

int main (int argc, char *argv[]) {
	
	//printf("\n\tUSER C Program:\n\n");
	
	//Reset random number each time user is ran
	time_t t;
	srand((int)time(&t) % getpid()); 
	
	//Access shared memory
	int shmidClock = shmget ( SHMKEY_CLK, BUFF_CLK_SZ, 0777); 
	int shmidPCB = shmget( SHMKEY_PCB, BUFF_PCB_SZ * maxProcesses, 0777); 	
	if (shmidClock == -1 || shmidPCB == -1){//check for errors
		perror("Error in shmget\n");
		exit (1);
    }
	
	//Get pointers to shared memory
	Clock (*systemClock) = shmat(shmidClock, 0, 0);	
	ProcessControlBlock (*PCB) = shmat(shmidPCB, 0, 0); 
	if((int *)systemClock == (int *) -1 || (int *)PCB == (int *) -1){ 
	//check for errors
		perror("Error in shmat\n");
		exit(1);
	}
	
	createMessageQueue(); //connect to message queue
	int processNum = atoi(argv[1]); //get argument

	/*
	//Verify information
	printf("\n\tInside User Process %d\n\n", processNum);
	printf("\t\tUSER: maxClaim = %d\n", PCB[processNum].maxClaims);
	printf("\t\tUSER: System Clock = %d:%d\n\n", systemClock->seconds, systemClock->nanoseconds);
	*/
	
	msg.processNum = processNum;
	
	int bound = 9; 
	int action;
	int resourceNum;
	int i;
	
	while(1){
		action = rand() % bound;
		
		if(action == 0){ //terminate process!!!!!!!
			msg.whatIdo = 0;
			msg.mtype = 1;
			//Send message so master knows of termination
			if(msgsnd(msgID, &msg, sizeof(msg), 0) == -1){
				perror("USER: Failed to send message");
			}
			break; //get out of while loop
		}
		else if(action < 6){ //even number means request!!!!!!!
			msg.whatIdo = 1;
			resourceNum = rand() % maxResources; //resources 0 ~ 19

			if(PCB[processNum].resourcesClaimed[resourceNum] <= PCB[processNum].maxClaims[resourceNum]){
				//Then the process can request another resource
				msg.resourceNum = resourceNum;
				
				msg.mtype = 1;
				//Send message only if we can request a resource!!!!!
				if(msgsnd(msgID, &msg, sizeof(msg), 0) == -1){
					perror("USER: Failed to send message");
				}
				//Wait for message
				if(msgrcv(msgID, &msg, sizeof(msg), 4, 1) == -1){
					perror("msgrcv"); //means there was an error
				} 
				else{
					//printf("\t\t*USER: Message Received!!!!!\n");
				}
			}
		}
		else{ //should be odd which means release!!!!!!
			msg.whatIdo = 2;
			//Find out which resource to release
			for(i = 0; i < maxResources; i++){
				if(PCB[processNum].resourcesClaimed[i] > 0){
					msg.resourceNum = i; //release first resource found
				
					msg.mtype = 1;
					//Send message only if we can release a resource!!!!!
					if(msgsnd(msgID, &msg, sizeof(msg), 0) == -1){
						perror("USER: Failed to send message");
					}
					
					//Wait for message
					if(msgrcv(msgID, &msg, sizeof(msg), 3, 1) == -1){
						perror("msgrcv"); //means there was an error
					} 
					else{
						//printf("\t\t*USER: Message Received!!!!!\n");
					}
					
					i = maxResources; //to get out of for loop
				}
			}
		}
		//Reset message and continue looping
		msg.mtype = 0;
	}
	
	return 0;
}