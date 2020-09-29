//Devin Bidstrup Fall 2020
//Give the library definitions
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#include "ec440threads.h"
#include "threads.h"

#define MAX_THREADS 128     //Max number of threads which can exist

//Define select global vars
struct threadControlBlock* tcb[MAX_THREADS];

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
    jmp_buf envBuffer;                   //threads state(Set of registers)
    void* stackArea;                    //Pointer to threads stack area
};

//Function to initialize the thread subsytem after the first call to pthread_create
void initializeThreadSS()
{
    //Put signal handler here (maybe) for alarm (RR scheduling)


    //Dynamically allocate the threadControlBlock array
    for (int i = 0; i < MAX_THREADS; i++)
    {
        tcb[i] = (struct threadControlBlock*) malloc(sizeof(struct threadControlBlock));
        tcb[i]->tid = -1;           //tid of -1 indicates an empty index of array
        tcb[i]->status = THREAD_DEAD;
    }
}

//pthread_create function
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                          void *(*start_routine) (void *), void *arg)
{
    //should run only for the first call to pthread_create
    initializeThreadSS();

    /*...Create thread context...*/
    //Find the first open thread ID and store that in currTid
    int currTid = 0;
    while(tcb[currTid]->tid != -1 && currTid < 128)
    {
        currTid++;
    }
    tcb[currTid]->tid = currTid;  //Storing the index shows that it is no longer empty

    //Allocate a new stack of 32,767 byte size
    void* stackPointer;
    stackPointer = malloc(32767);
    tcb[currTid]->stackArea = stackPointer;

    //Initialize the threads state with start_routine
    //use setjmp to save the state of the current thread in jmp_buf
    int val = setjmp(tcb[currTid]->envBuffer);
    if (val)
        printf("returned from longjmp\n");
    //Change the program counter(RIP) to point to the start_thunk function
        //Need to mangle before values can be changed in jmp_buf
        //RIP is index 7
    tcb[currTid]->envBuffer[7] = ptr_mangle(&start_thunk); 
    
    //set RSP(stack pointer) to the top of our newly allocated stack
        //RSP index is 6 in jmp_buf
    tcb[currTid]->envBuffer[6] = ptr_mangle(stackPointer);  
    
    //Change R13(index 3 of jmp_buf) to contain value of void* arg
    tcb[currTid]->envBuffer[3] = (long) arg;
    
    //Change R12(index 2 of jmp_buf) to contain value of start_routine
    tcb[currTid]->envBuffer[2] = &start_routine;
    
    //start_thunk should be used to create fake context
    start_thunk();

    //Place address of pthread_exit() at the top of the stack and move RSP
    *stackPointer = &pthread_exit;
    stackPointer -= sizeof(&pthread_exit);

    //Set status to ready
    tcb[currTid]->status = THREAD_READY;

    return 0;
}


extern void pthread_exit(void *value_ptr)
{
    exit(EXIT_FAILURE);
}


extern pthread_t pthread_self()
{
    pthread_t i;

    return i;
}