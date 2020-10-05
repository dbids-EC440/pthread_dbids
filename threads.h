/*Devin Bidstrup EC440 HW2 Fall 2020*/

//This is the header file which contains the functions created by the threads file
#include <pthread.h>

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