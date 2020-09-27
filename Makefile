GCC = gcc -g -Werror -Wall -std=c99

default: exec

exec: main.o threads.o
	$(GCC) -o exec main.o threads.o

main: main.c threads.h
	$(GCC) -c main.c -o main.o

threads: threads.c threads.h
	$(GCC) -c threads.c -o threads.o

clean: 
	rm threads.o main.o exec
