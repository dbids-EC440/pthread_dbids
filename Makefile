GCC = gcc -g -Werror -Wall -std=gnu11

all: threads

threads: threads.c threads.h ec440threads.h
	$(GCC) -c threads.c -o threads.o

clean: 
	rm threads.o
