//Devin Bidstrup Fall 2020q
//Give the library definitions
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include "ec440threads.h"
#include "threads.h"

#define MAX_THREADS 128     //Max number of threads which can exist
#define RR_INTERVAL 50000   //Number of seconds to wait between SIGALRM signals (50ms)

//jmp_buf index definition
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC  7

//Define different thread status'
enum threadStatus
{
    THREAD_RUNNING,         /*Running thread*/
    THREAD_READY,           /*Not running but ready to run*/
    THREAD_BLOCKED,         /*Waiting for i/o*/
    THREAD_DEAD,            /*Thread either not initialized or finished*/    
};

//Put thread control block here
struct threadControlBlock
{
    enum threadStatus status;           /*Thread status*/
    jmp_buf envBuffer;                  /*threads state(Set of registers)*/
    int exitStatus;                     /*Exit status of the thread*/
    int justCreated;                    /*bool used to determine if the thread was just created*/
};

//Define select global vars
struct threadControlBlock tcb[MAX_THREADS];
pthread_t globalTid = 0;
struct sigaction act;
int isMainThread = 0;
//struct timeval tval_before, tval_after, tval_result;

//Handler function to schedule processes in a round robin fashion
void scheduleHandler()
{
    //gettimeofday(&tval_after, NULL);
    //timersub(&tval_after, &tval_before, &tval_result);
    //printf("Time elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
        
    //Set current thread to READY and find next thread with Ready status
    switch(tcb[globalTid].status)
    {
        case THREAD_RUNNING:
        case THREAD_BLOCKED:
            tcb[globalTid].status = THREAD_READY;
            break;
        case THREAD_READY:
        case THREAD_DEAD:
            break;
    }

    int foundThread = 0;
    pthread_t currTid = globalTid;
    while(!foundThread)
    {
        //Loop through all possible gCurrent values
        if(currTid == MAX_THREADS - 1)
        {
            currTid = 0;
        }
        else
        {
            currTid++;
        }
        printf("currTid: %d | ", (int)currTid);
        //If a thread is ready, exit the loop
        if(tcb[currTid].status == THREAD_READY)
        {
            foundThread = 1;
            printf("\ncurrTid thread[%d] status: %d\n", (int)currTid, tcb[currTid].status);
        }
    }

    //jumpLoop var prevents infinite looping between setjmp and longjmp, 0 = normal return, nonzero = longjmp return 
    int jumpLoop = 0;

    if (tcb[globalTid].justCreated == 0  && tcb[globalTid].status != THREAD_DEAD)
    {
        //printf("globalTid before setjmp: %d\n", (int)globalTid);
        jumpLoop = setjmp(tcb[globalTid].envBuffer);
        //printf("globalTid after setjmp: %d\n", (int)globalTid);
    }
    
    //Needed to remove setjmp from happening the first time
    if (tcb[currTid].justCreated)
    {
        tcb[currTid].justCreated = 0;
        //printf("justCreated is zero for tid %d\n", (int) currTid);
    }

    //longjmp is used to restore the next thread and resume execution of that thread
    if (!jumpLoop)
    {
        globalTid = currTid;
        tcb[globalTid].status = THREAD_RUNNING;
        longjmp(tcb[globalTid].envBuffer, 1);
    }
    
    //printf("Exiting scheduler...\n");
}

//Function to initialize the thread subsytem after the first call to pthread_create
void initializeThreadSS()
{
    //Initialize all of the threads to be dead
    for (int i = 0; i < MAX_THREADS; i++)
    {
        tcb[i].status = THREAD_DEAD; 
    }

    //Setup the ualarm function to trigger SIGALRM signal every 50ms
    useconds_t usecs = RR_INTERVAL;
    useconds_t interval = RR_INTERVAL;
    ualarm(usecs, interval);
    
    //gettimeofday(&tval_before, NULL);

    //signal handler for alarm (RR scheduling)
    sigemptyset(&act.sa_mask);
    act.sa_handler = &scheduleHandler;
    act.sa_flags = SA_NODEFER;
    sigaction(SIGALRM, &act, NULL);
}

//Setup the calling thread
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                          void *(*start_routine) (void *), void *arg)
{

    static int firstTime = 1;               /*Define bool for the firstTime*/
    
    attr = NULL;                            /*Specifically define attr as NULL*/

    //should run only for the first call to pthread_create
    if (firstTime)
    {
        initializeThreadSS();
        firstTime = 0;

        //Saves the main threads context and checks to see if it's the main thread
        tcb[0].status = THREAD_READY;
        tcb[0].justCreated = 1;
        isMainThread = setjmp(tcb[0].envBuffer);
        //printf("isMainThread: %d\n", isMainThread);
    }

    /*...Create thread context...*/
    if (!isMainThread)
    {
        //Find the first open thread ID and store that in currTid
        //Note that I chose the thread id's to be 1-127, with 0 as the main thread
        pthread_t currTid = 1;
        while(tcb[currTid].status != THREAD_DEAD && currTid < MAX_THREADS)
        {
            currTid++;
        }
        
        //Store the thread id in the *thread pointer so that it is known to main
        *thread = currTid;

        //Initialize the threads state with start_routine
        //use setjmp to save the state of the current thread in jmp_buf
        //printf("pthread_create setjmp with tid: %d\n", (int)currTid);
        setjmp(tcb[currTid].envBuffer);

        //Change the program counter(RIP) to point to the start_thunk function
        tcb[currTid].envBuffer[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); 
        
        //set RSP(stack pointer) to the top of our newly allocated stack
        //tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);  
        
        //Change R13(index 3 of jmp_buf) to contain value of void* arg
        tcb[currTid].envBuffer[0].__jmpbuf[JB_R13] = (long) arg;
        
        //Change R12(index 2 of jmp_buf) to contain address of start_routine function
        tcb[currTid].envBuffer[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;
        
        //Allocate a new stack of 32,767 byte size
        void *stackPointer = malloc(32767);
        stackPointer += (32767);
        //Place address of pthread_exit() at the top of the stack and move RSP
        stackPointer -= sizeof(&pthread_exit);
        int addressSize = sizeof(&pthread_exit);
        stackPointer = memcpy(stackPointer, pthread_exit, addressSize);
        tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);

        //Set status to THREAD_RUNNING
        tcb[currTid].status = THREAD_READY;
        
        //Set just created to 1 for scheduleHandler
        tcb[currTid].justCreated = 1;

        /*printf("JB_PCd: %ld\n", ptr_demangle(tcb[currTid].envBuffer[0].__jmpbuf[JB_PC]));
        printf("JB_PC: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_PC]);
        printf("JB_RSPd: %ld\n", ptr_demangle(tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP]));
        printf("JB_RSP: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP]);
        printf("JB_R13: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_R13]);
        printf("JB_R12: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_R12]);*/

        scheduleHandler();
    }
    else
    {   
        isMainThread = 0;
    }
    
    return 0;
}

//Terminate the calling thread
extern void pthread_exit(void *value_ptr)
{
    //The first thread was returning with globalTid of 0, this fixes that error
    /*static int firstThreadError = 1;
    if (firstThreadError && globalTid == 0)
    {
        globalTid = 1;
        firstThreadError = 0;
    }*/
    
    printf("pthread_exit for tid %d with status of %d \n", (int) globalTid, (int)tcb[globalTid].status);

    tcb[globalTid].status = THREAD_DEAD;

    //printf("pthread_exit for tid %d with status of %d \n", (int) globalTid, (int)tcb[globalTid].status);

    //Check to see if there are any threads still waiting to finish
    int stillThreads = 0;
    for (int i = 0; i < MAX_THREADS; i++)
    {
        //If a thread is exists, set stillThreads to 1
        switch(tcb[i].status)
        {
            case THREAD_READY   :
            case THREAD_RUNNING :
            case THREAD_BLOCKED :
                stillThreads = 1;
                break;
            case THREAD_DEAD:
                break;
        }
    }
    //printf("stillThreads: %d\n", stillThreads);

    //If threads still exist then schedule the next one
    if (stillThreads)
    {
        scheduleHandler();
    }

    //else exit
    exit(0);
}

//Returns the thread id of the currently running thread
extern pthread_t pthread_self()
{
    return globalTid;
}