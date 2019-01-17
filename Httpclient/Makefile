VERSION=v1.0
prefix=/usr/local

CC = g++
CFLAGS  := -Wall -std=c++11 -g
CPPFLAGS += -fPIC
SHARE := -shared

OPEN_LIB = -lcurl

OBJS = LibcurClient.o   #mongoose.o 
LIB = libcurClient.so 

bindir=$(prefix)/bin

BINDIR=$(DESTDIR)$(bindir)
LIBDIR=/lib

all : $(LIB)

install: $(LIB)
	cp $(LIB) $(LIBDIR)

FORCE:
$(LIB) : $(OBJS)

	$(CC) $(CFLAGS) $(SHARE) -o $@ $(OBJS) $(OPEN_LIB)

clean:
	rm -f $(OBJS) $(LIB)

LibcurClient.o: LibcurClient.cpp $(INCRTMP) LibcurClient.h   
#mongoose.o: mongoose.c $(INCRTMP) mongoose.h


