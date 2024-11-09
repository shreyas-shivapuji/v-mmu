#include <bits/stdc++.h>
#include <unistd.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>

using namespace std;

typedef struct 
{
	int frame;
	bool valid;
	int time;	//time of last access of the page no
}PTentry;

typedef struct {
	int pid;
	int m;			//size of page table
	int allocount;	//number of frames allocated to the process
	int usecount;	//number of frames currently used by the process
}process;

typedef struct 
{
	int size;		//number of free frames available
	int ffl[];		//list of free frames
}FFL;

int k,m,f,s;
int key;

int PTid, FFLid;
int MQ1id, MQ2id, MQ3id;
int PCBid;
int master_pid,scheduler_pid,mmu_pid;

void clear_and_exit(int i)
	{
		//remove the shared memory segments and message queues

		if(shmctl(PTid,IPC_RMID,NULL)==-1)
			{
				perror("PT Shared Memory Error");			//Throw errors in case the destroy fails
			}
		if(shmctl(FFLid,IPC_RMID,NULL)==-1)
			{
				perror("FFL Shared Memory Error");
			}
		if(msgctl(MQ1id,IPC_RMID,NULL)==-1)
			{
				perror("MQ1 Error");
			}
		if(msgctl(MQ2id,IPC_RMID,NULL)==-1)
			{
				perror("MQ2 Error");
			}
		if(msgctl(MQ3id,IPC_RMID,NULL)==-1)
			{
				perror("MQ3 Error");
			}
		exit(i);
	}

void createFFL()
{
	//create free frame list
	int i;
	int *a;
	FFL *ptr;
	key=rand();
	FFLid=shmget(key,sizeof(FFL)+f*sizeof(int),0666|IPC_CREAT|IPC_EXCL);	//create FFL
	if(FFLid==-1)
	{	
		perror("FFL Shared Memory Error");
		clear_and_exit(1);
	}

	ptr=(FFL*)(shmat(FFLid, NULL, 0));
	a=(int *)ptr;
	if(*a==-1)
	{
		perror("FFL Shared Memory Attach Error");
		clear_and_exit(1);
	}

	for(i=0;i<f;i++)
	{
		ptr->ffl[i]=i;	//add the free frames to the list
	}

	ptr->size=f;	//initially number of free frames = f

	if(shmdt(ptr)==-1)
	{
		perror("FFL Shared Memory Detach Error");
		clear_and_exit(1);
	}
}

void createPT()
{
	//create page tables
	int i;
	key=rand();
	PTid=shmget(key,m*k*sizeof(PTentry),0666|IPC_CREAT|IPC_EXCL);
	if(PTid==-1)
	{	
		perror("PT Shared Memory Error");
		clear_and_exit(1);
	}

	PTentry *pt=(PTentry *)(shmat(PTid,NULL,0));
	int *a=(int *)pt;
	if(*a==-1)
	{
		perror("PT Shared Memory Attach Error");
		clear_and_exit(1);
	}
	//initialize the frame no to -1 and valid to false
	for(i=0;i<k*m;i++)
	{
		pt[i].frame=-1;
		pt[i].valid=false;
	}

	if(shmdt(pt)==-1)
	{
		perror("PT Shared Memory Detach Error");
		clear_and_exit(1);
	}
}

void createMQs()
{
	//create the 3 message queues
	key=rand();
	MQ1id=msgget(key,0666|IPC_CREAT|IPC_EXCL);			//create message queues using MQ1id,MQ2id,MQ3id
	if(MQ1id==-1)
	{
		perror("MQ1 Create Error");
		clear_and_exit(1);
	}

	key=rand();
	MQ2id=msgget(key,0666|IPC_CREAT|IPC_EXCL);
	if(MQ2id==-1)
	{
		perror("MQ2 Create Error");
		clear_and_exit(1);
	}

	key=rand();
	MQ3id=msgget(key,0666|IPC_CREAT|IPC_EXCL);
	if(MQ3id==-1)
	{
		perror("MQ3 Create Error");
		clear_and_exit(1);
	}
}

void createProcessBlocks()
{
	//create process control blocks to store the number of pages, number of frames allocated and used for each process
	int i;
	key=rand();
	PCBid=shmget(key,sizeof(process)*k,0666|IPC_CREAT|IPC_EXCL);
	if(PCBid==-1)
	{	
		perror("Process Block Create Error");
		clear_and_exit(1);
	}

	process *ptr=(process*)(shmat(PCBid, NULL, 0));
	int *a=(int *)ptr;
	if(*a==-1)
	{
		perror("Shared Memory Attach Error: PCB");
		clear_and_exit(1);
	}

	int totalpages=0;	//total no. of pages for all the processes
	for(i=0;i<k;i++)
	{
		ptr[i].pid=i;			
		ptr[i].m=rand()%m+1;	// No. of pages is a random number between 1 to m
		totalpages+=ptr[i].m;	
		ptr[i].usecount=0;	//initially no. of frames used is 0
	}

	int total_allo=0; //total no. of frames allocated
	int max = 0,maxi = 0;
	int allo;
	for(i=0;i<k;i++)
	{
		//allocate each process minimum 1 frame and then out of the remaining f-k frames, allocate frames in proportion
		//to the number of pages required for each process
		allo=1+(int)(ptr[i].m*(f-k)/(float)totalpages);
		ptr[i].allocount = allo;
		total_allo = total_allo + allo;
	}

	int remain_allo=f-total_allo;
	//allocate the remaining frames randomly
	while(remain_allo>0){
		ptr[rand()%k].allocount += 1;
		remain_allo--;
	}

	for(i=0;i<k;i++)
	{
		cout<<"Process ID = "<<ptr[i].pid<<", Size of Page Table = "<<ptr[i].m<<", Allocated No. of Frames = "<<ptr[i].allocount<<endl;
	}

	if(shmdt(ptr)==-1)
	{
		perror("Shared Memory Detach Error: PCB");
		clear_and_exit(1);
	}

}

void createProcesses()
{
	process *ptr = (process*)(shmat(PCBid, NULL, 0));
	
	int *a=(int *)ptr;
	if(*a==-1)
	{
		perror("Shared Memory Attach Error: PCB");
		clear_and_exit(1);
	}

	int i,j,seed;
	int rlen;
	int generated;
	int n,r;
	string ref;
	for(i=0;i<k;i++)
	{
		ref.clear();
		seed=time(NULL);
		rlen=rand()%(8*ptr[i].m+1)+2*ptr[i].m; //length of reference string is between 2m to 10m
		generated=0;

		while(generated!=rlen)
		{
			srand(generated);
			n=rand()%(rlen/3+1)+1;
			//in each iteration, seed srand with certain value and generate a part of reference string and then
			//again seed srand with same value and generate more page numbers. This ensures that a page accessed once
			//is again accessed soon.
			if(generated+n<=rlen)
			{
				if(rand()%2==0) srand(seed);
				for(j=0;j<n;j++)
				{
					r=rand()%ptr[i].m+rand()%n;
					ref=ref+to_string(r)+"  ";
				}
				generated+=n;
			}

			else
			{
				srand(seed);
				for(j=0;j<rlen-generated;j++)
				{
					r=rand()%ptr[i].m+rand()%(n*2);
					ref=ref+to_string(r)+"  ";
				}
				generated=rlen;
			}
		}

		cout<<"Reference String: "<<ref<<endl;
		if(fork()==0)
		{
			char PNo[20],M1[20],M3[20];
			sprintf(PNo,"%d",i);
			sprintf(M1,"%d",MQ1id);
			sprintf(M3,"%d",MQ3id);
			execlp("./process","./process",PNo,M1,M3,(char *)&ref[0],(char *)(NULL));		//call the process
			exit(0);

		}

		usleep(250*1000);
	}
}


void complete(int signo)
{
	sleep(1);
	if(signo==SIGUSR1)
		{
			kill(scheduler_pid,SIGTERM);	//send signal to scheduler to terminate
			kill(mmu_pid,SIGUSR2);			//send signal to MMU to terminate
			sleep(1);
			clear_and_exit(0);
		}
}

int main(int argc, char const *argv[])
{
	srand(time(NULL));
	signal(SIGUSR1,complete);
	signal(SIGINT,clear_and_exit);
	if(argc < 5)
	{
		cout<<"Error: 4 arguments needed: k, m, f, s\n";
		clear_and_exit(1);
	}

	k = atoi(argv[1]);		// No. of processes
	m = atoi(argv[2]);		// Max size of page table
	f = atoi(argv[3]);		// Total number of frames in main memory
	s = atoi(argv[4]);		// Size of TLB
	master_pid = getpid();

	if(k<= 0||m<= 0||f<=0||f<k)
	{
		cout<<"Input is invalid\n";
		clear_and_exit(1);
	}

	createPT();				//create the data structures required
	createFFL();
	createProcessBlocks();
	createMQs();

	scheduler_pid=fork();
	if(scheduler_pid==0)
			{
				char M1[20],M2[20],N[20],PID[20];
				sprintf(M1,"%d",MQ1id);
				sprintf(M2,"%d",MQ2id);
				sprintf(N,"%d",k);
				sprintf(PID,"%d",master_pid);
				execlp("./scheduler","./scheduler",M1,M2,N,PID,(char *)(NULL));		//create scheduler
				exit(0);
			}

	mmu_pid=fork();
	if(mmu_pid==0)
	{
		char buf1[20],buf2[20],buf3[20],buf4[20],buf5[20],buf6[20],buf7[20],buf8[20];
		sprintf(buf1,"%d",MQ2id);
		sprintf(buf2,"%d",MQ3id);
		sprintf(buf3,"%d",PTid);
		sprintf(buf4,"%d",FFLid);
		sprintf(buf5,"%d",PCBid);
		sprintf(buf6,"%d",m);
		sprintf(buf7,"%d",k);
		sprintf(buf8,"%d",s);
		execlp("xterm","xterm","-e","./mmu",buf1,buf2,buf3,buf4,buf5,buf6,buf7,buf8,(char *)(NULL));		//create MMU
		exit(0);
	}

	createProcesses();
	
	pause(); //wait till scheduler notifies completion of all processes

	clear_and_exit(0);
	return 0;
}