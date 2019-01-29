VERSION=v1.0
LIBDIR=/lib
CC = g++

CPPFLAGS := -g -Wall -fPIC -Wno-error=overflow -std=c++11
CFLAGS  := -Wall -std=c++11 -g

SHARE := -shared

OPEN_LIB = -lpthread  -lcurl

OBJS = LibcurClient.o CommonList.o MessageQueue.o 
LIB = libbase.so 

all : $(LIB)

install: $(LIB)
	cp $(LIB) $(LIBDIR)

FORCE:
$(LIB) : $(OBJS)

	$(CC) $(CFLAGS) $(CPPFLAGS) $(SHARE) -o $@ $(OBJS) $(OPEN_LIB)

clean:
	rm -f $(OBJS) $(LIB)

LibcurClient.o: LibcurClient.cpp LibcurClient.h
CommonList.o: CommonList.cpp  CommonList.h
MessageQueue.o: MessageQueue.c MessageQueue.h


