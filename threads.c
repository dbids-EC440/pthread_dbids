//Devin Bidstrup Fall 2020
//Give the library definitions
#include <pthread.h>

//Put thread control block here

//Put signal handler here

//Pointer mangle function used for writing pc and rsp before writing to jump buffer
unsigned long int ptr_mangle(unsigned long int p)
{
    unsigned long int ret;
    asm("movq %1, %%rax;\n"
        "xorq %%fs:0x30, %%rax;"
        "rolq $0x11, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );    
    return ret;
}

//Pointer demangle to remove the effects of the mangle function
unsigned long int ptr_demangle(unsigned long int p) 
{    
    unsigned long int ret;
    asm("movq %1, %%rax;\n"
        "rorq $0x11, %%rax;"
        "xorq %%fs:0x30, %%rax;"
        "movq %%rax, %0;"    
    : "=r"(ret)    
    : "r"(p)
    : "%rax"    
    );    
    return ret;
}

//start_thunk function used to send value of R13 to the first arguement of the function stored in R12
void *start_thunk() 
{  
    asm("popq %%rbp;\n"           //clean up the function prolog      
        "movq %%r13, %%rdi;\n"    //put arg in $rdi      
        "pushq %%r12;\n"          //push &start_routine      
        "retq;\n"                 //return to &start_routine      
        :      
        :      
        : "%rdi"  
    );  
    __builtin_unreachable();
}

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