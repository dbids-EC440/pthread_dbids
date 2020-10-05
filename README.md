# Project 2 README by Devin Bidstrup
### Written for Boston University EC440: Introduction to Operating Systems
This project is a thread libaray which implements the pthread API's create, exit, and self
functions.  Herein, I will go through the threads.c code in line order, and describe its functionality.

### Pre-processor directives
The `MAX_THREADS` is the value of 128 which was defined to be the maximum number of threads which
are allowed to run concurrently.  The `RR_INTERVAL` is the value of 50000 which represents the
time in micro seconds that the library will wait before triggering the SIGALRM signal.  The 
`JB_X` definitions decribe the specific indexes for the `jmp_buf` structure on the specific 
hardware that was being used for this implementation.

### Thread Control Block
This struct contins the information which is held for a given thread, i.e. its status, set of 
registers, exit status, and a bool which indicates if it was just created.  The status is stored
in the form of an enumeration, which contains one of four status's: `THREAD_RUNNING`, 
`THREAD_READY`, `THREAD_BLOCKED`, and `THREAD_DEAD`.  This status is changed by the library for 
a given thread as it is scheduled and its start routine is executed.  Additionally, the `jmp_buf`
envBuffer variable is used to store the registers neccessary for the function run by the thread 
to be stopped and resumed as the scheduler sees fit.

## `void scheduleHandler()`
This handler function happens on one of three conditions, it is called explicitly from 
`pthread_create`, it is called explicitly from `pthread_exit`, or it is called when the SIGALRM 
signal is sent.  First the scheduler will set the currently running function's (if it is 
currenly running) status to `THREAD_READY`.  Then it loops through all of the threads starting 
at the current thread, until it finds the next thread which is ready to continue execution. At 
that point it will save the state of the previous thread if it is not the first time the thread
has been in the scheduler and if the thread is not dead.  It is saved using the setjmp function
with the argument of the envBuffer from the thread control block struct.  Then the function will
use change the global thread id value to the new function, set its status to `THREAD_RUNNING` and
use the longjmp function to resume the functions execution.
:
## `int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void*arg)`
This is the first of three library functions called in main.  This creates the thread and 
prepares it in order to execute the function passed as one of the arguments.

### `initializeThreads()`
This function is called the first time that `pthread_create` is called by the main function.  I 
first sets the status of all threads to `THREAD_DEAD`.  Then it sets up the SIGALRM signal using 
the ualarm function, which will call the signal after the number of microseconds corresponding 
to the first argument, and will continue to send the signal every number of seconds 
corresponding to the second argument.  Then the signal handler for the function is setup using 
the act struct to store a function pointer to the scheduleHandler, after which the act struct
is passed to the sigaction system call to associate the SIGALRM alarm with the scheduleHandler
for the process.

### main thread context
After the initializeThreads() function is called from within the `pthread_create` function, the
function sets up the context for the main thread.  Since the main thread is already running this
is more simple than for future threads.  The thread id of 0 is reserved for the main thread.
At this pont the status of the thread is set to `THREAD_READY` in the appropriate thread control
block and the justCreated bool is set to true.  Then the setjmp function is used to save the 
state of the registers for the main function in the thread control block.  When the main function
is first schedules, the main thread will return to this setjmp and then proceed passed the new
thread setup, to the end of the `pthread_create` function where it will return to main.

### new thread context
First the the program searches for the next open thread id and stores it in the thread
argument of the `pthread_create` function.  Then it uses the setjmp function to store the 
state of the current thread, thereby allowing us to alter that state.  We do so by indexing
into the `jmp_buf` structure and changing the registers.  Ideally we would change the program
counter to point to start routine and pass it the argument of arg by changing rdi directly.
Since we wish to have the start routine function act as if it was preceeded by a call, we
instead use the `start_thunk` function to create a fake context for us.  Therefore, we change the
program counter of the envBuffer to point to `start_thunk` and change R13 to hold arg and R12
to hold start routine.  Note that we additionally need to mangle the pointer before changing the
program counter, due to a security feature present in the libc wrapper.  Finally we create a 
32,767 byte stack for our thread using the malloc function.  Then we change the stackPointer 
such that it points to the top of the stack.  Then we push the address of `pthread_exit()`
to the top of that stack, so that when `start_routine` returns, `pthread_exit` is implicitly
called.  Now the status of the thread is set to `THREAD_READY`, the justCreated bool is set to true
and the scheduleHandler() is explictly called.

## `void pthread_exit(void* value_ptr)`
This function when called, will terminate the calling thread.  First it sets the threads status
to `THREAD_DEAD`.  Then it runs a loop through all of the threads, to check if there are any
threads remaining to be executed.  If there are still threads then the schedule handler is invoked.  Otherwise the exit function is called to explicitly terminate the program.

## `pthread_t pthread_self()`
This returns the thread id of the calling thread based on the global thread id of the library at the time of its calling.
