VERSION=v1.0
prefix=/usr/local

CC = g++
CPPFLAGS := -g -Wall -fPIC -Wno-error=overflow -std=c++11
CFLAGS  := -Wall -std=c++11 -g

SHARE := -shared

OPEN_LIB = -lpthread

OBJS = CommonList.o
LIB = libbase.so 

bindir=$(prefix)/bin

BINDIR=$(DESTDIR)$(bindir)
LIBDIR=/lib

all : $(LIB)

install: $(LIB)
	cp $(LIB) $(LIBDIR)

FORCE:
$(LIB) : $(OBJS)

	$(CC) $(CFLAGS) $(CPPFLAGS) $(SHARE) -o $@ $(OBJS) $(OPEN_LIB)

clean:
	rm -f $(OBJS) $(LIB)

CommonList.o: CommonList.cpp $(INCRTMP) CommonList.h   


