VERSION=v1.0
CC =gcc
CFLAGS  := -Wall -g -fPIC
SHARE := -shared
LIBDIR=/lib


OBJS = message_queue.o 
LIB = libmsgqueue.so 

all : $(LIB)

install: $(LIB)
	cp $(LIB) $(LIBDIR)

FORCE:
$(LIB) : $(OBJS)

	$(CC) $(CFLAGS) $(SHARE) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIB)

message_queue.o: message_queue.c $(INCRTMP) message_queue.h   



