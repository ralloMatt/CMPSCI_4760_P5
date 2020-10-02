#define SHMKEY_CLK 823566 // key for clock
#define SHMKEY_PCB 823567 // key for PCB

#define BUFF_CLK_SZ	sizeof ( Clock ) //size of memory used for clock
#define BUFF_PCB_SZ	sizeof ( ProcessControlBlock ) //size of a PCB

#define maxResources 20 //max number of resources
#define MSG_KEY 2013

int maxProcesses = 0; //set in oss.c when I get user input

typedef struct Clock{ //implemented system clock
	int seconds;
	int nanoseconds;
}Clock;

typedef struct ProcessControlBlock{ //used to keep info on processes
	int processNum; //number given to that process when first made
	pid_t pid; //process id
	int blockedTimeSecs; //time spent being blocked
	int blockedTimeNano;
	int maxClaims[maxResources]; //the max amount of a resource that process can claim
	int resourcesClaimed[maxResources]; //index is the resource and the value is the amount allocated to this process
	int blocked; //if 1 the process was blocked!
	int deniedResource; //the resource that was denied!
}ProcessControlBlock;

typedef struct ResourceDesc{
	int instances; //how many instances of the resource are available
	int allocated; //current allocation to processes from this resource
}ResourceDesc;

typedef struct Message{
	long mtype;
	int whatIdo; //0 then terminate, 1 then request, 2 then release
	int resourceNum; // a particular resource a process wants
	int processNum; //so I know which process is sending the message
}Message;

int msgID; //message queue id
Message msg;

void createMessageQueue(){
	if ((msgID = msgget(MSG_KEY, IPC_CREAT | 0666)) == -1) {
      perror("msgget");
      exit(1);
	}
}