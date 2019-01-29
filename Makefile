VERSION=v2.0

prefix=/usr/local

CC = g++
CFLAGS  := -Wall -std=c++11 -g -Wno-write-strings
CPPFLAGS += -fPIC

bindir=$(prefix)/bin

BINDIR=$(DESTDIR)$(bindir)

RECORDSAVE= RecordSave/libRecordSave.so
BASE= Base/libbase.so

INCRTMP= RecordSave/RecordSaveRunnable.h  Base/base.h
OPEN_LIB = -lcurl -lRecordSave -lglog -lpthread -lbase

MONITOR = recordMonitor
SERVER = recordServer

MOBJS = monitor.o
OBJS = webserver.o  mongoose.o

TARGET = $(MONITOR) $(SERVER)

all : $(HTTPCLIENT) $(RECORDSAVE) $(COMMON) $(TARGET)

install:	$(TARGET)
	cp $(TARGET) $(BINDIR)
	@cd Base; $(MAKE) install
	@cd RecordSave; $(MAKE) install

FORCE:

$(BASE): FORCE
	@cd Base; $(MAKE) all

$(RECORDSAVE): FORCE
	@cd RecordSave; $(MAKE) all

$(MONITOR) : $(MOBJS)
	$(CC) $(CFLAGS) $(SHARE) -o $@ $(MOBJS) $(OPEN_LIB)

$(SERVER) : $(OBJS)
	$(CC) $(CFLAGS) $(SHARE) -o $@ $(OBJS) $(OPEN_LIB)

clean:
	rm -f $(OBJS) $(MOBJS) $(TARGET)
	@cd Base; $(MAKE) clean
	@cd RecordSave; $(MAKE) clean

webserver.o: webserver.c webserver.h
mongoose.o: mongoose.c mongoose.h
monitor.o: monitor.c
