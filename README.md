# Project 3 README by Devin Bidstrup
### Written for Boston University EC440: Introduction to Operating Systems
In this section of the project, the pthread_join function, a system to lock and unlock threads, as well as POSIX semaphore functionality (sem_init, sem_wait, sem_post, sem_destroy) were implemented adding to the threading library previously developed.
Appended to the end of this README, is that of the previous project, which might be needed to understand the implementation I describe.

## `extern void lock()`
This function simply uses the sigproc mask function to add to the SIG_BLOCK mask for the thread the SIGALRM signal.  This in effect allows the thread to not be scheduled out and continue running until the `extern void unlock` function is used.

## `extern void unlock()`
This function simply uses the sigproc mask function to remove from the SIG_BLOCK mask for the thread the SIGALRM signal.  This in effect allows the thread to resume being scheduled after the `extern void lock` function disabled that functionality.

## `extern int pthread_join(pthread_t thread, void** value_ptr)`
This function suspends the execution of the calling thread until the target thread terminates, unless it has already terminated.  In reality the functionality of the thread is dependent on changes to the pthread_exit and pthread_create functions in order to work, to make this more concise these changes shall be detailed here.  
In pthread_create, the address of pthread_exit is no longer pushed directly onto the threads stack.  Instead, the pthread_exit_wrapper function is pushed onto the threads stack.  By the power of inline x86 assembly, this function loads the return value of the thread into the value_ptr argument of pthread_exit.  
Within the pthread_exit function, the threads status is now set to THREAD_DEAD instead of THREAD_EMPTY.  This new status indicates that a thread has finished executing, but its context has been preserved in case of a future call to pthread_join with that thread as the target.  The value_ptr argument is then placed in the exist status member of the tcb for that thread.  pthread_exit no longer cleans up the threads context as long as there are remaining threads (only if all threads are finished does it free their respective stacks).
The pthread_join function itself first determines the status of the target thread with a switch statement.  If it is READY, RUNNING, or BLOCKED then the thread blocks until the thread has the THREAD_DEAD status.  This is achieved by setting its status to THREAD_BLOCKED and the waitingTid value of the target threads tcb to the current thread.  Then the scheduleHandler function is called to schedule out the thread.  The THREAD_DEAD case of the thread is reached two ways.  Either the thread was dead from the start, and the switch statement causes the program to jump there, or originally the thread was blocked waiting for the target to finish execution, and has just returned from the scheduleHandler call that it made previously.  Either way the exit status of the target thread is stored in the value_ptr void**.  Then the target threads context is cleaned up, meaning that its status is set to THREAD_EMPTY and its stack is freed.  If the function is successful it should return 0.

### semaphoreControlBlock
This struct contains additional information needed in order to implement the POSIX semaphore functionality.  The first member stores the value of the semaphore as an integer.  The second stores a pointer to a Queue struct which lists the threads blocked on the semaphore at a given point in time.  This queue is implemented in the queueHeader.h file.  The majority of the code for that queue was found on the GfG website, and is by no means my own work.  Only the slight modification of changing the data stored from int to pthread_t was neccessary to get it working for my implementation.  The third member is a flag which is true if the semaphore has been initialized, but is otherwise false.  This was neccessary to prevent the sem_destroy function removing a semaphore which had yet to be initialized.

## `int sem_init(sem_t *sem, int pshared, unsigned value)`
This function initializes a semaphore pointed to by sem to the value of value.  For our purposes, the pshared argument will always be zero, indicating that sem is shared between processes.  First space is allocated for a semaphoreControlBlock using the malloc function and stored in scbPtr.  Then that scb's value is changed to the value passed to sem_init, the blockedThreads queue created (but still empty), and the isInitialized flag is set to 1 (true).  Finally that scb is saved within the `__align` part of sem_t structure for sem.

## `int sem_wait(sem_t *sem)`
This function either descrements the semaphore referenced by sem, or blocks the thread until that semaphore can be decremented.  First the function gets the scb for this semaphore from the `__align` member of sem_t.  This is used to check the value of the semaphore.  If the value is <= 0 then the calling thread sets its status to THREAD_BLOCKED, and adds itself to the blocked threads queue of the scb before calling the scheduleHandler to schedule out this blocked thread.  Else if the value > 0  intially then the thread decrements the semaphore and returns 0.  There is a new member of the tcb which was added to indicate when a thread was woken up in sem_post.  So that if the thread returns from the scheduleHandler (i.e. unblocks), the thread should be caught by the following if statement which checks if woke is 1 for the threads tcb.  If so it sets the woke member to zero and returns 0.  Else, there was some error and -1 is returned.

## `int sem_post(sem_t *sem)`
This function increments the value of the semaphore if there are no threads waiting for the semaphore to post, and otherwise wakes the first thread waiting in the queue.  First the function gets the scb for this semaphore from the `__align` member of sem_t.  This is then used to check the value of the semaphore.  If the value is >= 0 then the function checks if there are threads waiting in the scb for this semaphore.  If no threads are waiting (-1 is returned) then the semaphores value is incremented in the scb.  If there are waiting threads, then the woke member of the threads tcb is set to 1, and the threads status is changed to THREAD_READY so that is will be scheduled in the future.  Then 0 is returned if the value of the semaphore is greater than zero, otherwise -1 is returned to indicate an error.

## `int sem_destroy(sem_t *sem)`
This function destroys the semaphore.  This is accomplished by first getting the scb for this semaphore from the `__align` member of sem_t.  Then use that to determine if the thread has been initialized.  If so, free the scb from memory, and return 0;  If not return -1 to indicate an error.



### Undefined Functionality
The following functionality is undefined, and should be avoided when using this implementation
* calling pthread_join with a target thread which has not been created, or has been fully removed (status of THREAD_EMPTY)
* attempting to initialize an already initialized semaphore
* Destroying a semaphore that other threads are blocked on
* using a semaphore that has been destroyed
* multiple calls to pthread_join on the same target


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
registers, exit status, a pointer to the stack and a bool which indicates if it was just
created.  The status is stored in the form of an enumeration, which contains one of four
status's: `THREAD_RUNNING`,  `THREAD_READY`, `THREAD_BLOCKED`, and `THREAD_DEAD`.  This status 
is changed by the library for a given thread as it is scheduled and its start routine is
executed.  Additionally, the `jmp_buf`envBuffer variable is used to store the registers
neccessary for the function run by the thread to be stopped and resumed as the scheduler sees
fit. 

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
argument of the `pthread_create` function.  If there are already 127 running threads and main, then 
the program will print an error message and exit.  Then it uses the setjmp function to store the 
state of the current thread, thereby allowing us to alter that state.  We do so by indexing
into the `jmp_buf` structure and changing the registers.  Ideally we would change the program
counter to point to start routine and pass it the argument of arg by changing rdi directly.
Since we wish to have the start routine function act as if it was preceeded by a call, we
instead use the `start_thunk` function to create a fake context for us.  Therefore, we change the
program counter of the envBuffer to point to `start_thunk` and change R13 to hold arg and R12
to hold start routine.  Note that we additionally need to mangle the pointer before changing the
program counter, due to a security feature present in the libc wrapper.  Finally we create a 
32,767 byte stack for our thread using the malloc function.  Then we change the get a pointer 
such that it points to the top of the stack.  Then we push the address of `pthread_exit()`
to the top of that stack, so that when `start_routine` returns, `pthread_exit` is implicitly
called.  Now the status of the thread is set to `THREAD_READY`, the justCreated bool is set to 
true and the scheduleHandler() is explictly called.

## `void pthread_exit(void* value_ptr)`
This function when called, will terminate the calling thread.  First it sets the threads status
to `THREAD_DEAD` and frees the stack for the thread.  Then it runs a loop through all of the threads,
to check if there are any threads remaining to be executed.  If there are still threads then the
schedule handler is invoked.  Otherwise the exit function is called to explicitly terminate the program.

## `pthread_t pthread_self()`
This returns the thread id of the calling thread based on the global thread id of the library at the time of its calling.
