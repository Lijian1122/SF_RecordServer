/*****************************************************
版权所有:北京三海教育科技有限公司
作者：lijian
版本：V0.0.1
时间：2018-09-17
功能：利用librtmp庫生成RecordSave.so,接收rtmp传输來的数据保存成.h264文件,.acc文件,.json文件

V0.0.2
2019.01.02 重新整理Rtmp断线重连逻辑，解决断线重连时候软件崩溃的bug，主要在接收线程解析flv 头部flag没有初始化
2019.01.08 修改Rtmp接收数据缓冲区，队列改为环形缓冲区，解决服务在解析Tag数据崩溃的bug
2019.01.09 重新整理代码
2019.01.17 优化rtmp断开重连代码
2019.01.25 新增处理rtmp连接时 拉流地址失败的异常

v0.0.3
2019.07.05 增加rtmp重连失败，删除空文件
2019.07.05 新增录制停止flag,解决获取最后录制数据不全的bug

v0.0.4
2019.07.08 增加录制停止flag,解决录制数据不全的问题

v0.0.5
2019.07.11 重新整理与http整理逻辑，监控直播客户端和上传录制状态
******************************************************/

#ifndef RECORDSAVERUNNABLE_H
#define RECORDSAVERUNNABLE_H

#include <sys/types.h> 
#include <sys/stat.h>  
#include "../Base/common.h"
#include "../Base/base.h"
#include "glog/logging.h"
#include "CCycleBuffer.h"
 
#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00)| (x << 8 & 0xff0000) | (x << 24 & 0xff000000))
#define UN_ABS(a,b) (a>=b?a-b:b-a)

extern string ServerPort,FILEFOLDER,IpPort,APIStr,record_serverId,ServerCreate,ServerDelete,ServerSelect,ServerUpdate,liveUpdate,liveSelect,liveUpload;
extern int aacTagCount;

using json = nlohmann::json;

extern "C"
{
  #include "librtmp/rtmp_sys.h"
  #include "librtmp/log.h"
}

typedef struct AdtsHeader
{
	unsigned char SamplIndex1: 3;
	unsigned char OBjecttype: 5;//2
	unsigned char other: 3;//000
	unsigned char channel: 4;
	unsigned char SamplIndex2: 1;
}AdtsHeader;

typedef struct AdtsData
{
	unsigned char check1;
	unsigned char protection : 1;//误码校验1
	unsigned char layer : 2;//哪个播放器被使用0x00
	unsigned char ver : 1;//版本 0 for MPEG-4, 1 for MPEG-2
	unsigned char check2 : 4;
	unsigned char channel1 : 1;
	unsigned char privatestream : 1;//0
	unsigned char SamplingIndex : 4;//采样率
	unsigned char ObjectType : 2;
	unsigned char length1 : 2;
	unsigned char copyrightstart : 1;//0
	unsigned char copyrightstream : 1;//0
	unsigned char home : 1;//0
	unsigned char originality : 1;//0
	unsigned char channel2 : 2;
	unsigned char length2;
	unsigned char check3 : 5;
	unsigned char length3 : 3;
	unsigned char frames : 2;//超过一块写
	unsigned char check4 : 6;
}AdtsData;

//Http接口返回值类型
enum URL_TYPE{ 
   SELECT_LIVEURL = 0,
   SELECT_LIVFLAG,
   UPDATA_RECORDFLAG
};

//录制状态
enum RECORD_FLAG{ 
   RECORD_START = 1,   //开始录制
   RECORD_STOP,        //停止录制
   RECORD_UPDATE,      //录制中 
   RECORD_CLIENT_ERROR,  //客户端推流异常而停止录制
   RECORD_CONNECT_SERVER_ERROR, //录制服务拉流异常停止录制
   RECORD_CLIENT_TIMEOUT   //客户端直播超时
};

//直播状态
enum LIVE_FLAG{
   LIVE_INIT = 0,    //初始状态 
   LIVE_START = 1,   //开始直播
   LIVE_STOP,        //停止直播
   LIVE_UPDATE,      //直播中
   LIVE_SERVER_ERROR,  //录制服务拉流异常 
   LIVE_CLIENT_ERROR, //客户端推流异常
   LIVE_TIME_OUT      //客户端直播超时
   
};

using namespace std;
class RecordSaveRunnable
{
	
public:
    RecordSaveRunnable(const char *pdata);
	
    ~RecordSaveRunnable();

    //启动录制任务
    int StartRecord(); 
    
    //设置停止录制flag
    void SetStopFlag(){stopflag = true;};
	
     //停止录制,等待读写线程退出
    int StopRecord();
	
    //获取直播ID
    string GetRecordID(){return m_recordID;};

protected:

    //读线程
    static void *Recive_fun(void* arg);
	void *rtmpRecive_f();
	
	//写线程
	static void *Save_fun(void *arg);
	void *rtmpSave_f();
	
	//重连rtmp初始化
    int BrokenlineReconnectionInit(RTMP *m_pRtmp, int re_Connects);
	
	//重连rtmp,如重连大于三次，结束录制任务 否则继续重连
    int BrokenlineReconnection(int re_Connects);
	
private:

    //获取完整一个Tag数据写文件
    int WriteFile(const char *tagHead_buf, const char *tagData_buf, int tagdataSize);

    //h264数据写入h264文件
    int Write264data(const char *timebuff,const char *packetBody, int datasize);

    //aac数据写入aac文件
    int WriteAac(const char *timebuff ,const char *packetBody, int datasize);

    //自定义白板数据写入json文件
    bool WriteExtractDefine(const char *timebuff ,const char *packetBody,  int datasize);

    //新建文件及文件夹
    int CreateFileDir(const char *sPathName);
    int CreateFile();

    //解析http返回json
    int ParseJsonInfo(std::string &jsonStr ,std::string &resCodeInfo,std::string &liveinfo ,std::string &pullUrl ,URL_TYPE urlflag);

    //更新录制状态
    int UpdataRecordflag(LibcurClient *recive_http ,int flag);
	
     //上传白板数据
    int UploadWhiteData(LibcurClient *recive_http ,std::string data);
	
     //写线程结束上传录制完成状态
    void UploadRecordStopFlag();

private:

   /*录制直播ID*/
   string m_recordID;

   /*音频解析所需要数据结构*/
   int iHasAudioSpecificConfig;
   AdtsHeader ah;
   AdtsData ad; 

   /*环形缓冲区对象*/
   CCycleBuffer *m_cycleBuffer;

   /*读线程*/
   pthread_t producter_t;  
   
   /*写线程*/
   pthread_t consumer_t;  
  
   /*读线程运行flag*/
   int runningp; 
   
   /*写线程运行flag*/  
   int runningc; 

   /*解析FLV头flag*/
   bool firstflag;  

   /*rtmp对象*/
   RTMP *m_pRtmp;  

   /*rtmp读数据缓冲区*/
   char *buf;
   
   /*读rtmp数据是否结束flag*/
   bool m_endRecvFlag; 

   /*返回值*/
   int m_ret;

   /*LibcurClient对象*/
   LibcurClient *recive_http; 
   LibcurClient *save_http;

   /*是否允许写线程上传录制状态flag*/
   bool save_httpflag;  
   
   /*上传录制状态flag*/
   int recive_httpflag;  
   
   /*aacTagNum计数，用来定时上传录制状态*/
   int aacTagNum;

   /*文件*/
   FILE *afile;
   FILE *vfile;
   FILE *wfile; 

   /*flv文件*/
   FILE *flvfile;
   
   /*拉流URL*/
   string  m_pullUrl;
   
   /*时间戳固定四个字节*/
   int timestampSize;

   /*停止录制标志*/
   bool stopRecordflag; 

   /*文件名字*/
   string aacfileName;
   string h264fileName;
   string whitefileName;
   string flvfileName;

   /*停止录制标志位*/
   bool stopflag;

   /*查询到直播状态的时间戳*/
   string liveFlagTime;
   string currentTimestamp;

   /*rtmp读数据超时标志位*/
   bool rtmpReadTimeout;
};

#endif // RECORDSAVERUNNABLE_H
