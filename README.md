# Project 2 README by Devin Bidstrup
### Written for Boston University EC440: Introduction to Operating Systems
This project is a thread libaray which implements the pthread API's create, exit, and self functions.  
Herein, I will go through the threads.c code in line order, and describe its functionality.

### Pre-processor directives
The MAX_THREADS is the value of 128 which was defined to be the maximum number of threads which are allowed 
to run concurrently.  The RR_INTERVAL is the value of 50000 which represents the time in micro seconds 
that the library will wait before triggering the SIGALRM signal.  The JB_X definitions decribe the 
specific indexes for the jmp_buf structure on the specific hardware that was being used for this implementation.

### Thread Control Block
This struct contins the information which is held for a given thread, i.e. its status, set of registers, 
exit status, and a bool which indicates if it was just created.  The status is stored in the form of an enumeration,
which contains one of four status's: THREAD_RUNNING, THREAD_READY, THREAD_BLOCKED, and THREAD_DEAD.  This status is
changed by the library for a given thread as it is scheduled and its start routine is executed.  Additionally, the
jmp_buf envBuffer variable is used to store the registers neccessary for the function run by the thread to be
stopped and resumed as the scheduler sees fit.

## `void scheduleHandler()`
This handler function happens on one of three conditions, it is called explicitly from pthread_create, it is called
explicitly from pthread_exit, or it is called when the SIGALRM signal is sent.  First the scheduler will set the 
currently running function's (if it is currently running) status to THREAD_READY.  Then it loops through all of the
threads starting at the current thread, until it finds the next thread which is ready to continue execution. At
that point it will save the state of the previous thread if it is not the first time the thread has been in the scheduler
and if the thread is not dead.  It is saved using the setjmp function with the argument of the envBuffer from the thread
control block struct.  Then the function will use change the global thread id value to the new function, set its 
status to THREAD_RUNNING and use the longjmp function to resume the functions execution.

## pthread_create
This is the first of three library functions called in main.  This creates the thread and prepares it in order to execute
the function passed as one of the arguments.

### initializeThreads()
This function is called the first time that pthread_create is called by the main function.  It first sets the status
of all threads to THREAD_DEAD.  Then it sets up the SIGALRM signal using the ualarm function, which will call the 
singal after the number of microseconds corresponding to the first argument, and will continue to send the signal
every number of seconds corresponding to the second argument.  Then the signal handler for the function is setup 
such that 
