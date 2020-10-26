/*Devin Bidstrup EC440 HW2 Fall 2020*/

//Give the library definitions
#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <errno.h>

#include "ec440threads.h"
#include "threads.h"
#include "queueHeader.h"

#define MAX_THREADS 128     /*Max number of threads which can exist*/
#define RR_INTERVAL 50000   /*Number of seconds to wait between SIGALRM signals (50ms)*/
#define STACK_SIZE  32767   /*Number of bytes in a threads stack*/

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
    THREAD_DEAD,            /*Thread finished, with context not cleaned up*/    
    THREAD_EMPTY            /*Thread context clean*/
};

//TCB which holds information for a given thread
struct threadControlBlock
{
    enum threadStatus status;           /*Thread status*/
    jmp_buf envBuffer;                  /*threads state(Set of registers)*/
    void* exitStatus;                   /*Exit status of the thread*/
    int justCreated;                    /*bool used to determine if the thread was just created*/
    void* stack;                        /*Pointer used to free the stack for a thread in pthread_exit*/
    pthread_t waitingTid;               /*Used to store the tid of a thread waiting for this one to finish*/
    int woke;                           /*bool use to determine if thread was just woken*/
};

//Struct which holds additional information needed for the semaphore implementation
struct semaphoreControlBlock
{
    int value;                          /*Number of wakeup calls to semaphore*/
    struct Queue* blockedThreads;       /*Pointer to queue of waiting threads, queue implemented in queueHeader*/
    int isInitialized;                  /*Flag indicating whether the semaphore is initialized*/
};

//Used to save the return value of a threads start_routine function
void pthread_exit_wrapper()
{
    unsigned long int res;
    asm("movq %%rax, %0\n":"=r"(res));
    pthread_exit((void *) res);
}

//Define select global variables
struct threadControlBlock tcbArray[MAX_THREADS];        /*Holds information for all of the threads*/
pthread_t globalTid = 0;                                /*Stores currently running threads tid*/
struct sigaction act;                                   /*Used to setup SIGALRM */

//Handler function to schedule processes in a round robin fashion
void scheduleHandler()
{        
    //Set currently running thread to READY
    switch(tcbArray[globalTid].status)
    {
        case THREAD_RUNNING:
            tcbArray[globalTid].status = THREAD_READY;
            break;
        case THREAD_BLOCKED:
        case THREAD_READY:
        case THREAD_DEAD:
        case THREAD_EMPTY:
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
        if(tcbArray[currTid].status == THREAD_READY)
        {
            foundThread = 1;
        }
    }

    int jumpLoop = 0;   /*jumpLoop var prevents infinite looping between setjmp and longjmp, 
                            0 = normal return, nonzero = longjmp return */
    
    //If the thread was not justCreated and is alive, then save the state
    if (tcbArray[globalTid].justCreated == 0  && tcbArray[globalTid].status != THREAD_DEAD)
    {
        jumpLoop = setjmp(tcbArray[globalTid].envBuffer);
    }
    
    //Needed to remove setjmp from happening the first time
    if (tcbArray[currTid].justCreated)
    {
        tcbArray[currTid].justCreated = 0;
    }

    //longjmp is used to restore the next thread and resume execution of that thread
    if (!jumpLoop)
    {
        globalTid = currTid;
        tcbArray[globalTid].status = THREAD_RUNNING;
        longjmp(tcbArray[globalTid].envBuffer, 1);
    }
}

//Function to initialize the thread subsytem after the first call to pthread_create
void initializeThreads()
{
    //Initialize all of the threads to be empty
    int i;
    for (i = 0; i < MAX_THREADS; i++)
    {
        tcbArray[i].status = THREAD_EMPTY; 
        tcbArray[i].waitingTid = i;
        tcbArray[i].justCreated = 0;
        tcbArray[i].woke = 0;
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
        initializeThreads();
        firstTime = 0;

        //Saves the main threads context and checks to see if it's the main thread
        tcbArray[0].status = THREAD_READY;
        tcbArray[0].justCreated = 1;
        isMainThread = setjmp(tcbArray[0].envBuffer);
    }

    /*...Create new thread context...*/
    if (!isMainThread)
    {
        //Find the first open thread ID and store that in currTid
        //Note that I chose the thread id's to be 1-127, with 0 as the main thread
        pthread_t currTid = 1;
        while(tcbArray[currTid].status != THREAD_EMPTY && currTid < MAX_THREADS)
        {
            currTid++;
        }

        //Check for an attempt to call more than 128 threads at once
        if (currTid == MAX_THREADS)
        {
            fprintf(stderr, "ERROR: Cannot call pthread_create, maximum number of threads reached\n");
            exit(EXIT_FAILURE);
        }
        
        //Store the thread id in the *thread pointer so that it is known to main
        *thread = currTid;

        //use setjmp to save the state of the current thread in jmp_buf
        setjmp(tcbArray[currTid].envBuffer);

        //Change the program counter(RIP) to point to the start_thunk function
        tcbArray[currTid].envBuffer[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); 
        
        //Change R13(index 3 of jmp_buf) to contain value of void* arg
        tcbArray[currTid].envBuffer[0].__jmpbuf[JB_R13] = (long) arg;
        
        //Change R12(index 2 of jmp_buf) to contain address of start_routine function
        tcbArray[currTid].envBuffer[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;
        
        //Allocate a new stack of 32,767 byte size and place a pointer at the top of the stack
        tcbArray[currTid].stack = malloc(STACK_SIZE);
        void* stackBottom = tcbArray[currTid].stack + STACK_SIZE;

        //Place address of pthread_exit() at the top of the stack and move RSP
        void* stackPointer = stackBottom - sizeof(&pthread_exit_wrapper);
        void (*tempAddress)(void*) = &pthread_exit_wrapper;
        stackPointer = memcpy(stackPointer, &tempAddress, sizeof(tempAddress));

        //set RSP(stack pointer) to the top of our newly allocated stack
        tcbArray[currTid].envBuffer[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);

        //Set status to THREAD_READY
        tcbArray[currTid].status = THREAD_READY;
        
        //Set just created to 1 for scheduleHandler to skip setjmp
        tcbArray[currTid].justCreated = 1;

        //Set the waitingTid to the process in case this is second use of same tid
        tcbArray[currTid].waitingTid = currTid;

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
    tcbArray[globalTid].status = THREAD_DEAD;

    //Set the threads exit status to value_ptr
    tcbArray[globalTid].exitStatus = value_ptr;

    //Deal with waiting process if there is one waiting
    pthread_t wTid = tcbArray[globalTid].waitingTid;
    if (wTid != globalTid)
    {
        tcbArray[wTid].status = THREAD_READY;
    }

    //Check to see if there are any threads still waiting to finish
    int stillThreads = 0;
    int i;
    for (i = 0; i < MAX_THREADS; i++)
    {
        //If a thread is exists, set stillThreads to 1
        switch(tcbArray[i].status)
        {
            case THREAD_READY   :
            case THREAD_RUNNING :
            case THREAD_BLOCKED :
                stillThreads = 1;
                break;
            case THREAD_DEAD:
            case THREAD_EMPTY:
                break;
        }
    }

    //If threads still exist then schedule the next one
    if (stillThreads)
    {
        scheduleHandler();
    }

    //Else exit the program
    //Cleanup dead threads contexts
    for (i = 0; i < MAX_THREADS; i++)
    {
        if (tcbArray[i].status == THREAD_DEAD)
        {
            free(tcbArray[i].stack);
        }
    }
    exit(0);
}

//Returns the thread id of the currently running thread
extern pthread_t pthread_self()
{
    return globalTid;
}

//Stops the calling thread from being interuppted by another thread
extern void lock()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

//Called after lock, allows the thread to be interuppted from then on
extern void unlock()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

//Postpones the execution of the calling thread until the target thread terminates
extern int pthread_join(pthread_t thread, void** value_ptr)
{
  
    switch(tcbArray[thread].status)
    {
        case THREAD_READY   :
        case THREAD_RUNNING :
        case THREAD_BLOCKED :
            //Set the status to Blocked and add it to the waiting queue of the target                
            tcbArray[globalTid].status = THREAD_BLOCKED;
            tcbArray[thread].waitingTid = globalTid;

            //Block until the target terminates
	        scheduleHandler();
            //At some point once the thread is finished, it will be sent back here
            
        case THREAD_DEAD:
            //Get the exit status of the target if value_ptr isn't NULL
            if (value_ptr)
                *value_ptr = tcbArray[thread].exitStatus;

            //Clean up the targets context
            tcbArray[thread].status = THREAD_EMPTY;
            free(tcbArray[thread].stack);
            break;

        case THREAD_EMPTY:
            printf("ERROR: Undefined behavoir\n");
            return 3; //ESRCH 3 No such process
            break;
    }
    return 0;
}

/*..................Semaphore library..................*/

//Intializes the semaphore referenced by sem
int sem_init(sem_t *sem, int pshared, unsigned value)
{
    //Initialize semaphore control block to store addtional information needed by the semaphore
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*) malloc(sizeof(struct semaphoreControlBlock));

    scbPtr->value = value;
    scbPtr->blockedThreads = createQueue();
    scbPtr->isInitialized = 1;

    //Save that semaphore within the sem_t struct
    sem->__align = (long) scbPtr;
    
    return 0;
}

//Decrements the semphore referenced by sem
int sem_wait(sem_t *sem)
{
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*)(sem->__align);

    //If the value is less or equal to than zero, block
    if (scbPtr->value <= 0)
    {
        tcbArray[globalTid].status = THREAD_BLOCKED;
        enQueue(scbPtr->blockedThreads, globalTid);
        scheduleHandler();
    }
    //This either happens immediately
    else
    {
        (scbPtr->value)--;
        return 0;
    }

    //This will happen if the semaphore is woken
    if (tcbArray[globalTid].woke)
    {
        tcbArray[globalTid].woke = 0;
        return 0;
    }

    //Return statement just in case error occurs
    return -1;
}

//increments the semphore referenced by sem
int sem_post(sem_t *sem)
{
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*)(sem->__align);

    if (scbPtr->value >= 0)
    {
        pthread_t waitingTid = deQueue(scbPtr->blockedThreads);
        if (waitingTid != -1)
        {
            //wakeup waiting thread
            tcbArray[waitingTid].woke = 1;
            tcbArray[waitingTid].status = THREAD_READY;
        }
        else
        {
            //If no thread was waiting, just increment the semaphore
            (scbPtr->value)++;
        }

        return 0;
    }
    
    return -1;
}

//destroys the semphore referenced by sem
int sem_destroy(sem_t *sem)
{
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*)(sem->__align);

    if (scbPtr->isInitialized == 1)
    {
        free((void *)(sem->__align));   //Free's the semaphore control block for this semaphore
    }
    else
    {
        return -1; //Error
    }
    
    return 0;
}
