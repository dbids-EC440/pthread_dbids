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
#define RR_INTERVAL 50000   //Number of seconds to wait between SIGALRM signals

//jmp_buf index definition
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

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
    pthread_t tid;                      //Thread ID
    enum threadStatus status;           //Thread status
    jmp_buf envBuffer;                  //threads state(Set of registers)
    int exitStatus;                     //Exit status of the thread
};

//Define select global vars
struct threadControlBlock tcb[MAX_THREADS];
pthread_t gCurrent;
struct sigaction act;
//struct timeval tval_before, tval_after, tval_result;

//Handler function to schedule processes in a round robin fashion
void scheduleHandler()
{
    int handlerCalled = 1;
    //gettimeofday(&tval_after, NULL);
    //timersub(&tval_after, &tval_before, &tval_result);
    //printf("Time elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);

    //setjmp is called to store the currently executing threads information
    setjmp(tcb[gCurrent].envBuffer);

    //Detects if the handler itself was called, or the function came back from setjmp
    if(handlerCalled)
    {
        //Set current thread to READY and find next thread with Ready status
        tcb[gCurrent].status = THREAD_READY;
        int foundThread = 0;
        while(!foundThread)
        {
            //Loop through all possible gCurrent values
            if(gCurrent == MAX_THREADS - 1)
            {
                gCurrent = 0;
            }
            else
            {
                gCurrent++;
            }
            
            //If a thread is ready, exit the loop
            if(tcb[gCurrent].status == THREAD_READY)
            {
                foundThread = 1;
                printf("thread[%d] status: %d\n", (int)gCurrent, tcb[gCurrent].status);
            }
        }

        //longjmp is used to restore the next thread and resume execution of that thread
        handlerCalled = 0;
        longjmp(tcb[gCurrent].envBuffer, 1);
    }
}

//Function to initialize the thread subsytem after the first call to pthread_create
void initializeThreadSS()
{
    //Initialize all of the threads to be dead
    for (int i = 0; i < MAX_THREADS; i++)
    {
        tcb[i].tid = 0;
        tcb[i].status = THREAD_DEAD; 
    }

    //Setup the ualarm function
    useconds_t usecs = RR_INTERVAL;
    useconds_t interval = RR_INTERVAL;
    ualarm(usecs, interval);
    
    //gettimeofday(&tval_before, NULL);

    //Set current thread id to gCurrent
    gCurrent = 1;

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
    //Define bool for the firstTime
    static int firstTime = 1;

    //should run only for the first call to pthread_create
    if (firstTime)
        initializeThreadSS();

    /*...Create thread context...*/
    //Find the first open thread ID and store that in currTid
    //Note that I chose the thread id's to be 1-127, with 0 as the main thread
    pthread_t currTid = 1;
    while(tcb[currTid].status != THREAD_DEAD && currTid < 128)
    {
        currTid++;
    }
    tcb[currTid].tid = currTid;  //Storing the index shows that it is no longer empty
    *thread = currTid;           //Stores the thread id in the pointer so that it is known to main

    //Allocate a new stack of 32,767 byte size
    void *stackPointer;
    stackPointer = malloc(32767);

    //Initialize the threads state with start_routine
    //use setjmp to save the state of the current thread in jmp_buf
    setjmp(tcb[currTid].envBuffer);

    //Change the program counter(RIP) to point to the start_thunk function
    tcb[currTid].envBuffer[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); 
    
    //set RSP(stack pointer) to the top of our newly allocated stack
    tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);  
    
    //Change R13(index 3 of jmp_buf) to contain value of void* arg
    tcb[currTid].envBuffer[0].__jmpbuf[JB_R13] = (long) arg;
    
    //Change R12(index 2 of jmp_buf) to contain address of start_routine function
    tcb[currTid].envBuffer[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;

    //Place address of pthread_exit() at the top of the stack and move RSP
    stackPointer = memcpy(stackPointer, pthread_exit, sizeof(pthread_exit));
    stackPointer += sizeof(pthread_exit);
    tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);

    //Set status to THREAD_RUNNING
    tcb[currTid].status = THREAD_READY;

    /*printf("JB_PCd: %ld\n", ptr_demangle(tcb[currTid].envBuffer[0].__jmpbuf[JB_PC]));
    printf("JB_PC: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_PC]);
    printf("JB_RSPd: %ld\n", ptr_demangle(tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP]));
    printf("JB_RSP: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP]);
    printf("JB_R13: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_R13]);
    printf("JB_R12: %ld\n", tcb[currTid].envBuffer[0].__jmpbuf[JB_R12]);*/

    //If first time, set to false and schedule
    if(firstTime)
    {
        firstTime = 0;
    }

    scheduleHandler();

    return 0;
}

//Terminate the calling thread
extern void pthread_exit(void *value_ptr)
{
    printf("pthread_exit for tid %d\n", (int) gCurrent);

    tcb[gCurrent].tid = 0;
    tcb[gCurrent].status = THREAD_DEAD;

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
    return gCurrent;
}