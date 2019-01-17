VERSION=v1.0

prefix=/usr/local

CC = g++
CFLAGS  := -Wall -std=c++11 -g -Wno-write-strings
CPPFLAGS += -fPIC

bindir=$(prefix)/bin

BINDIR=$(DESTDIR)$(bindir)

HTTPCLIENT= Httpclient/libcurClient.so
RECORDSAVE= RecordSave/RecordSave.so

INCRTMP= RecordSave/RecordSaveRunnable.h  Httpclient/LibcurClient.h
OPEN_LIB = -lmsgqueue -lcurl -lcurClient -lRecordSave -lglog -lpthread


MONITOR = recordMonitor
SERVER = recordServer

MOBJS = monitor.o
OBJS = webserver.o  mongoose.o

TARGET = $(MONITOR) $(SERVER)

all : $(HTTPCLIENT) $(RECORDSAVE) $(TARGET)

install:	$(TARGET)
	cp $(TARGET) $(BINDIR)
	@cd Httpclient; $(MAKE) install
	@cd RecordSave; $(MAKE) install

FORCE:

$(HTTPCLIENT): FORCE
	@cd Httpclient; $(MAKE) all

$(RECORDSAVE): FORCE
	@cd RecordSave; $(MAKE) all

$(MONITOR) : $(MOBJS)
	$(CC) $(CFLAGS) $(SHARE) -o $@ $(MOBJS) $(OPEN_LIB)

$(SERVER) : $(OBJS)
	$(CC) $(CFLAGS) $(SHARE) -o $@ $(OBJS) $(OPEN_LIB)

clean:
	rm -f $(OBJS) $(MOBJS) $(TARGET)
	@cd Httpclient; $(MAKE) clean
	@cd RecordSave; $(MAKE) clean

webserver.o: webserver.c webserver.h
mongoose.o: mongoose.c mongoose.h
monitor.o: monitor.c