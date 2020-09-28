//Devin Bidstrup Fall 2020
//Give the library definitions
#include <pthread.h>
#include <ec440threads.h>

#define MAX_THREADS 128     //Max number of threads which can exist

//Define different thread status'
enum threadState
{
    THREAD_RUNNING,         /*Running thread*/
    THREAD_READY,           /*Not running but ready to run*/
    THREAD_BLOCKED          /*Waiting for i/o*/
};

//Put thread control block here
struct threadControlBlock
{
    pthread_t tid;                      //Thread ID
    enum threadState status;            //Thread status
                                        //threads state(Set of registers)
                                        //Program Counter
    void* stackArea;                    //Pointer to threads stack area
    pid_t* ppid;                        //Pointer to process that created this thread
};

//Function to initialize the thread subsytem after the first call to pthread_create
void initializeThreadSS()
{
    //Put signal handler here (maybe) for alarm (RR scheduling)


    //Dynamically allocate the threadControlBlock array
    struct threadControlBlock* tcb[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; i++)
    {
        tcb[i] = (struct threadControlBlock*) malloc(sizeof(struct threadControlBlock));
    }
}

//pthread_create function
extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                          void *(*start_routine) (void *), void *arg)
{
    intializeThreadSS();

    //Allocate a new stack of 32,767 byte size
    void* stackPointer;;
    stackPointer = malloc(32767);

    //Initialize the threads state with start_routine
        //use setjmp to save the state of the current thread in jmp_buf
        //change the program counter(RIP) to point to the start_thunk function
        //set RSP(stack pointer) to the top of our newly allocated stack

    return 0;
}


extern void pthread_exit(void *value_ptr)
{

}


extern pthread_t pthread_self()
{
    pthread_t i;

    return i;
}