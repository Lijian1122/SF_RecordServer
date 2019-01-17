/*****************************************************
版权所有:北京三海教育科技有限公司
作者：lijian
版本：V0.0.1
时间：2018-09-17
功能：利用rtmpdump庫生成RecordSave.so,接收rtmp传输來的数据保存成.h264文件,.acc文件,.json文件

2019.01.02 重新整理Rtmp断线重连逻辑，解决断线重连时候软件崩溃的bug，主要在接收线程解析flv 头部flag没有初始化
2019.01.08 修改Rtmp接收数据缓冲区，队列改为环形缓冲区，解决服务在解析Tag数据崩溃的bug
2019.01.09 重新整理代码
******************************************************/

#ifndef RECORDSAVERUNNABLE_H
#define RECORDSAVERUNNABLE_H

#include "../Httpclient/LibcurClient.h"
#include "../json.hpp"
#include "glog/logging.h"
#include "CCycleBuffer.h"

using json = nlohmann::json;

extern "C"
{
  #include "librtmp/rtmp_sys.h"
  #include "librtmp/log.h"
  #include "../webserver.h"
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


class RecordSaveRunnable
{
	
public:
    RecordSaveRunnable(char *pdata);
	
    ~RecordSaveRunnable();

    //启动录制任务
    int StartRecord(); 
    
	//停止录制任务
    int StopRecord();

protected:

    //读线程
    static void *Recive_fun(void* arg);
	void *rtmpRecive_f();
	
	//写线程
	static void *Save_fun(void *arg);
	void *rtmpSave_f();
	
	//断线重连初始化
    int BrokenlineReconnectionInit(RTMP *m_pRtmp);
   
private:

    //获取完整一个Tag数据写文件
	int WriteFile(char *tagHead_buf, char *tagData_buf, int tagdataSize);

    //h264数据写入h264文件
    int Write264data(char *timebuff,char *packetBody, int datasize);

    //aac数据写入aac文件
    int WriteAac(char *timebuff ,char *packetBody, int datasize);

    //自定义白板数据写入json文件
    bool WriteExtractDefine(char *timebuff ,char *packetBody,  int datasize);

    //新建文件及文件夹
    int CreateFileDir(const char *sPathName);
    int CreateFile(std::string &resData);

    //解析http返回json
    int ParseJsonInfo(std::string &jsonStr ,std::string &resCodeInfo,std::string &liveinfo ,std::string &pullUrl ,bool urlflag);

    //更新录制状态
    int UpdataRecordflag(LibcurClient *recive_http ,int flag);
	
	//上传白板数据
    int UploadWhiteData(LibcurClient *recive_http ,std::string data);

public:

   /*录制ID*/
   string m_recordID;
   
private:

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
   LibcurClient *upload_http;

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
   
   /*拉流URL*/
   string  m_pullUrl;
   
   /*时间戳固定四个字节*/
   int timestampSize; 
};

#endif // RECORDSAVERUNNABLE_H
