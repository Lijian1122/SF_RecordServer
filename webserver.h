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

v 0.0.4
2019.05.23 添加配置文件
2019.05.23 修改监控进程功能

v 0.0.5
2019.05.28 配置文件增加ServerCreate配置项
******************************************************/
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/vfs.h> 
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "Base/common.h"
#include "mongoose.h"
#include "glog/logging.h"
#include "Base/base.h"
#include "Base/CommonList.h"
#include "RecordSave/RecordSaveRunnable.h"

extern string ServerPort, FILEFOLDER,IpPort,APIStr, record_serverId,ServerCreate,ServerDelete,ServerSelect,ServerUpdate,liveUpdate,liveSelect,liveUpload;
extern int aacTagCount;

int httpSev_flag = 1; //http服务线程退出标志
int recordMange_flag = 1; //任务管理线程退出标志
int record_flag = 1; //录制服务在线状态上传标志

/*string APIStr="/live/record";
string HttpAPIStr="http://192.168.1.205:8080/live/";

string updateOnlineUrl;  //更新录制在线Url
string LOGFOLDER =  "./recordlog/";
string ServerName = "LIVE录像01";*/

string HttpAPIStr,updateOnlineUrl,LOGFOLDER,ServerName,ServerNameAPIStr, ServerCreateStr;

//Http服务返回值枚举
enum RESCODE{ 
   NO_ERROR = 0, 
   TYPE_ERROR,
   LIVEID_ERROR,
   METHOD_ERROR,
   MALLOC_ERROR,
   URL_ERROR   
};

//定时器任务类型
enum TIMER_TYPE{ 
   UPDATEONLINE = 0,
   CHEDISK
};

//录制命令枚举
enum RECORDCMD{
    START = 0, //开始录制
    STOP   //停止录制
};

//Http接口返回值类型
enum PARSE_TYPE{ 
   GETAPI = 0,
   REGISTONLINE,
   UPDATA
};

//直播参数结构
typedef struct liveParmStruct
{
   string liveID;
   RECORDCMD cdmType;
}liveParmStruct;

//直播参数队列  直播对象队列 删除对象队列
CommonList *LiveParmList, *RecordSaveList, *DeleteRecordList;

//读取配置文件对象
CConfigFileReader config_file("server.conf");

//线程对象
pthread_t recordManage_t;
pthread_t httpServer_t;
pthread_t httpTime_t;
pthread_t checkDisk_t;
pthread_t deletRecordTask_t;

//Http请求对象
LibcurClient *m_httpclient, *s_httpclient;


//处理Http请求
void ev_handler(struct mg_connection *nc, int ev, void *ev_data);

//处理录制参数线程
void *recordManage_fun(void *data);

//停止录制任务 线程
void *stopRecord_fun(void *data);

//解析Http返回json数据
int parseResdata(string &resdata,PARSE_TYPE m_Type);

//设置定时器任务
void setTimer(unsigned seconds ,TIMER_TYPE TimerFlag);

//定时检测磁盘 线程
void *checkDisk_fun(void *data);

//定时上传录制在线 线程
void *httpTime_fun(void *pdata);

//上传录制在线
void updateOnline_fun();

//检测磁盘空间
void checkdisk_fun();

//http服务监听 线程
void *httpServer_fun(void *pdata);

//创建日志文件夹
int CreateLogFileDir(const char *sPathName);

//启动服务
int startServer(void);

//停止服务
int stopServer(void);

#endif


