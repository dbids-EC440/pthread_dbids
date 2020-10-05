/*Devin Bidstrup EC440 HW2 Fall 2020*/

//Give the library definitions
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "ec440threads.h"
#include "threads.h"

#define MAX_THREADS 128     //Max number of threads which can exist
#define RR_INTERVAL 50000   //Number of seconds to wait between SIGALRM signals (50ms)

//jmp_buf index definitions
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

//TCB which holds information for a given thread
struct threadControlBlock
{
    enum threadStatus status;           /*Thread status*/
    jmp_buf envBuffer;                  /*threads state(Set of registers)*/
    int exitStatus;                     /*Exit status of the thread*/
    int justCreated;                    /*bool used to determine if the thread was just created*/
};

//Define select global variables
struct threadControlBlock tcb[MAX_THREADS];         /*Holds information for all of the threads*/
pthread_t globalTid = 0;                            /*Stores currently running threads tid*/
struct sigaction act;                               /*Used to setup SIGALRM */

//Handler function to schedule processes in a round robin fashion
void scheduleHandler()
{        
    //Set currently running thread to READY
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

    //Loop through threads to find the next thread to schedule
    int foundThread = 0;
    pthread_t currTid = globalTid;
    while(!foundThread)
    {
        //Check if at the end of the loop, otherwise increment
        if(currTid == MAX_THREADS - 1)
        {
            currTid = 0;
        }
        else
        {
            currTid++;
        }
        
        //If a thread is ready, exit the loop
        if(tcb[currTid].status == THREAD_READY)
        {
            foundThread = 1;
        }
    }

    int jumpLoop = 0;   /*jumpLoop var prevents infinite looping between setjmp and longjmp, 
                            0 = normal return, nonzero = longjmp return */
    
    //If the thread was not justCreated and is alive, then save the state
    if (tcb[globalTid].justCreated == 0  && tcb[globalTid].status != THREAD_DEAD)
    {
        jumpLoop = setjmp(tcb[globalTid].envBuffer);
    }
    
    //Needed to remove setjmp from happening the first time
    if (tcb[currTid].justCreated)
    {
        tcb[currTid].justCreated = 0;
    }

    //longjmp is used to restore the next thread and resume execution of that thread
    if (!jumpLoop)
    {
        globalTid = currTid;
        tcb[globalTid].status = THREAD_RUNNING;
        longjmp(tcb[globalTid].envBuffer, 1);
    }
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

    //signal handler for alarm (Round Robin scheduling)
    sigemptyset(&act.sa_mask);
    act.sa_handler = &scheduleHandler;
    act.sa_flags = SA_NODEFER;
    sigaction(SIGALRM, &act, NULL);
}

//Setup the calling thread from main
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                          void *(*start_routine) (void *), void *arg)
{

    static int firstTime = 1;               /*Define bool for the firstTime*/
    int isMainThread = 0;                   /*Used to stop prevent thread context creation for the main thread*/
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
    }

    /*...Create new thread context...*/
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

        //use setjmp to save the state of the current thread in jmp_buf
        setjmp(tcb[currTid].envBuffer);

        //Change the program counter(RIP) to point to the start_thunk function
        tcb[currTid].envBuffer[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); 
        
        //Change R13(index 3 of jmp_buf) to contain value of void* arg
        tcb[currTid].envBuffer[0].__jmpbuf[JB_R13] = (long) arg;
        
        //Change R12(index 2 of jmp_buf) to contain address of start_routine function
        tcb[currTid].envBuffer[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;
        
        //Allocate a new stack of 32,767 byte size
        void *stackPointer = malloc(32767);
        stackPointer += (32767);

        //Place address of pthread_exit() at the top of the stack and move RSP
        stackPointer -= sizeof(&pthread_exit);
        void (*tempAddress)(void*) = &pthread_exit;
        int addressSize = sizeof(&pthread_exit);
        stackPointer = memcpy(stackPointer, &tempAddress, addressSize);

        //set RSP(stack pointer) to the top of our newly allocated stack
        tcb[currTid].envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);

        //Set status to THREAD_READY
        tcb[currTid].status = THREAD_READY;
        
        //Set just created to 1 for scheduleHandler to skip setjmp
        tcb[currTid].justCreated = 1;

        //I chose to call scheduleHandler explictly
        scheduleHandler();
    }
    else
    {   
        //If it isMainThread then it should set the bool to zero and return back to main
        isMainThread = 0;
    }
    
    return 0;
}

//Terminate the calling thread
extern void pthread_exit(void *value_ptr)
{
    //Set threads status to dead
    tcb[globalTid].status = THREAD_DEAD;

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
    return globalTid;
}