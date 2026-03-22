CC = gcc
CFLAGS = -Wall
LIBS = -lcurl -lcjson

SRCS = main.c server.c network.c utils.c
OBJS = $(SRCS:.c=.o)

main: $(OBJS)
	$(CC) $(OBJS) -o main $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) main

.PHONY: clean