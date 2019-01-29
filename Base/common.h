#ifdef _COMMON_H
#define _COMMON_H
#include <string>
#include <string.h>
#include <stdlib.h> 
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <iostream>
#include <algorithm>
#include <termio.h>
#include <malloc.h>
#include <sys/select.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/stat.h> 
#include <signal.h>
#include <stdio.h>
#include <dirent.h>

extern const char *s_http_port;
extern string FILEFOLDER;
extern string IpPort;
extern string record_serverId;

//Http API方法名
extern string ServerCreate;
extern string ServerDelete;
extern string ServerSelect;
extern string ServerUpdate;

extern string liveUpdate;
extern string liveSelect;
extern string liveUpload;

#endif
