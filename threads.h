/*Devin Bidstrup EC440 HW2 Fall 2020*/

//This is the header file which contains the functions created by the threads file
#include <pthread.h>
#include <semaphore.h>

/*............Project2............*/
//Setup the calling thread from main
extern int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine) (void *),
    void *arg
);

//Terminate the calling thread
extern void pthread_exit(void *value_ptr);

//Returns the thread id of the currently running thread
extern pthread_t pthread_self();

/*............Project3............*/
//Stops the calling thread from being interuppted by another thread
extern void lock();

//Called after lock, allows the thread to be interuppted from then on
extern void unlock();

//Postpones the execution of the calling thread until the target thread terminates
extern int pthread_join(pthread_t thread, void** value_ptr);

//Semaphore library

//Intializes the semaphore referenced by sem
int sem_init(sem_t *sem, int pshared, unsigned value);

//decrements the semphore referenced by sem
int sem_wait(sem_t *sem);

//increments the semphore referenced by sem
int sem_post(sem_t *sem);

//destroys the semphore referenced by sem
int sem_destroy(sem_t *sem);