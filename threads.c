//Give the library definitions
#include <pthread.h>


extern int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                          void *(*start_routine) (void *), void *arg)
{
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