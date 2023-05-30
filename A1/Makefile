CC = gcc
CFLAGS = -g -Wall

all: shell

pcb.o: shell_error.h pcb.h pcb.c
	$(CC) $(CFLAGS) -c pcb.c

command.o: shell_error.h pcb.o command.h command.c
	$(CC) $(CFLAGS) -c command.c

shell: shell_error.h pcb.o command.o shell379.c
	$(CC) $(CFLAGS) -o shell.exe shell379.c pcb.o command.o

clean:
	rm *.o