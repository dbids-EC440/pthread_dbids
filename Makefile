GCC = gcc -g -Werror -Wall 

all: threads

threads: threads.c threads.h ec440threads.h
	$(GCC) -c threads.c -o threads.o

clean: 
	rm threads.o
