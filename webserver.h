/*****************************************************
版权所有:北京三海教育科技有限公司
作者：lijian
版本：V0.0.1
时间：2018-09-18
功能：录制服务所有功能，完成录制服务的所有功能

v 0.0.2
2019.01.15 重构录制服务,添加定时器，完成定时删除录制对象功能
2019.01.15 Http服务返回值用枚举类型代替

v 0.0.3
2019.01.16 将定时任务进行统一处理，枚举来代替不同的定时类型
2019.01.18 将录制任务删除定时器机制改为事件机制，用线程同步来实现即时删除录制任务任务
2019.01.25 将任务对象存储数据结构改为list，将list的出队入队操作封装为一个类
******************************************************/

#include <string.h>
#include <stdlib.h> 
#include <dlfcn.h>
#include <pthread.h>
#include <semaphore.h>
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

#define CONTENTTYPE "Content-Type: application/x-www-form-urlencoded\r\n"  

#define IPPORT "http://192.168.1.205:8080/live/"

#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00)| (x << 8 & 0xff0000) | (x << 24 & 0xff000000))

#define UN_ABS(a,b) (a>=b?a-b:b-a)

#define FILEFOLDER "./recordFile/"

#define APIStr "/live/record"

static const char *s_http_port = "8081";

static int httpSev_flag = 1; //http服务线程退出标志

static int recordMange_flag = 1; //任务管理线程退出标志

static int record_flag = 1; //录制服务在线状态上传标志

string record_serverId;  //录制服务ID

string updateOnlineUrl;  //更新录制在线Url

string  LOGFOLDER =  "./recordlog/";

string  serverName = "LIVE录像01";

string IpPort = "http://";

//Http API方法名
string ServerCreate;
string ServerDelete;
string ServerSelect;
string ServerUpdate;

string liveUpdate;
string liveSelect;
string liveUpload;

//Http服务返回值枚举
enum RESCODE{ 
   NO_ERROR = 0, 
   TYPE_ERROR,
   LIVEID_ERROR,
   METHOD_ERROR,
   MALLOC_ERROR   
};

//定时器任务类型
enum TIMER_TYPE{ 
   UPDATEONLINE = 0,
   CHEDISK
};

//Http接口返回值类型
enum PARSE_TYPE{ 
   GETAPI = 0,
   REGISTONLINE,
   UPDATA
};

//直播参数结构体
typedef struct liveParmStruct
{
   char *liveID;
   char *liveType;
}liveParmStruct;


