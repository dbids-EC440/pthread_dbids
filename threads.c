//Devin Bidstrup Fall 2020q
//Give the library definitions
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

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
struct threadControlBlock* tcb[MAX_THREADS];
pthread_t gCurrent;
struct sigaction act;

//Handler function to schedule processes in a round robin fashion
void scheduleHandler()
{
    //Find the currently executing thread by searching for THEAD_RUNNING status
    
    //setjmp is called to store the currently executing threads information

    //longjmp is used to restore the next thread and resume execution of that thread
}

//Function to initialize the thread subsytem after the first call to pthread_create
void initializeThreadSS()
{
    //Dynamically allocate the threadControlBlock array
    for (int i = 0; i < MAX_THREADS; i++)
    {
        //tcb[i] = (struct threadControlBlock*) malloc(sizeof(struct threadControlBlock));
        tcb[i]->tid = -1;           //tid of -1 indicates an empty index of array
        tcb[i]->status = THREAD_DEAD;
    }

    //Setup the ualarm function
    useconds_t usecs = RR_INTERVAL;
    useconds_t interval = RR_INTERVAL;
    ualarm(usecs, interval);

    //Set current thread id to gCurrent
    gCurrent = 0;

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
    pthread_t currTid = 0;
    while(tcb[currTid]->tid != -1 && currTid < 128)
    {
        currTid++;
    }
    tcb[currTid]->tid = currTid;  //Storing the index shows that it is no longer empty
    *thread = currTid;            //Stores the thread id in the pointer so that it is know to main

    //Allocate a new stack of 32,767 byte size
    void (*stackPointer)(void*);
    stackPointer = malloc(32767);

    //Initialize the threads state with start_routine
    //use setjmp to save the state of the current thread in jmp_buf
    setjmp(tcb[currTid]->envBuffer);

    //Change the program counter(RIP) to point to the start_thunk function
    tcb[currTid]->envBuffer[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); 
    
    //set RSP(stack pointer) to the top of our newly allocated stack
    tcb[currTid]->envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);  
    
    //Change R13(index 3 of jmp_buf) to contain value of void* arg
    tcb[currTid]->envBuffer[0].__jmpbuf[JB_R13] = (long) arg;
    
    //Change R12(index 2 of jmp_buf) to contain address of start_routine function
    tcb[currTid]->envBuffer[0].__jmpbuf[JB_R12] = (unsigned long int) &start_routine;
    
    //start_thunk should be used to create fake context
    start_thunk();

    //Place address of pthread_exit() at the top of the stack and move RSP
    stackPointer = &pthread_exit;
    stackPointer -= sizeof(&pthread_exit);
    tcb[currTid]->envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);

    //If first Process set status to THREAD_RUNNING, otherwise set to THEAD_READY
    tcb[currTid]->status = (firstTime) ? THREAD_RUNNING : THREAD_READY;

    //Set first time to false if true
    if (firstTime) firstTime = 0;

    //Execute start routine and check for return
    start_routine(arg);

    return 0;
}

//Terminate the calling thread
extern void pthread_exit(void *value_ptr)
{
    exit(EXIT_FAILURE);
}

//Returns the thread id of the currently running thread
extern pthread_t pthread_self()
{
    return gCurrent;
}