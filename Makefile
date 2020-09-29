GCC = gcc -g -Werror -Wall -std=gnu11

all: main threads
	$(GCC) -o main main.o threads.o

main: main.c threads.h
	$(GCC) -c main.c -o main.o

threads: threads.c threads.h ec440threads.h
	$(GCC) -c threads.c -o threads.o

clean: 
	rm threads.o main.o main
