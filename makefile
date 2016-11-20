CC = gcc
CCFLAGS = -Wall -lpthread -Werror -g -o
SRCS = serverdaemon.c accelerometer.c
all:
	$(CC) $(SRCS) $(CCFLAGS) serverdaemon.out

clean:
	 rm serverdaemon.out
