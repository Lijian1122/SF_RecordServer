#include "RecordSaveRunnable.h"

RecordSaveRunnable::RecordSaveRunnable(const char *pdata)
{
 
     m_recordID = pdata;

     recive_http = new LibcurClient;

     save_http = new LibcurClient;

     upload_http= new LibcurClient;

     save_httpflag = true; 

     recive_httpflag = 0;

     iHasAudioSpecificConfig = 0;
     ah = {0};
     ad = {0}; 
	 
     m_cycleBuffer = new CCycleBuffer(BUFFER_SIZE);
	
     runningp = 1;  
     runningc = 1;   

     firstflag = true;  

     aacTagNum = 0;

     buf = NULL;
     m_pRtmp= NULL;

     afile = NULL;
     vfile = NULL;
     wfile = NULL;
     flvfile = NULL;
     timestampSize = 4;

     //初始化写线程停止状态
     stopRecordflag = false;

     //初始化http停止录制命令状态
     stopflag = false;
}


//解析Http返回json
int RecordSaveRunnable::ParseJsonInfo(std::string &jsonStr ,std::string &resCodeInfo ,std::string &liveinfo ,std::string &pullUrl,URL_TYPE urlflag)
{
    int main_ret = 0;

    if(!jsonStr.empty())
    {  
        json m_object = json::parse(jsonStr);
        if(m_object.is_object())
        {
           string resCode = m_object.value("code", "oops");
           main_ret = atoi(resCode.c_str() );
           if(0 == main_ret)
           {
			  resCodeInfo = m_object.value("msg", "oops");				  
			  switch(urlflag) 
              {
		         case URL_TYPE::UPDATA_RECORDFLAG:  //更新录制状态
	             {
					  break;
				 }
				 case URL_TYPE::SELECT_LIVEURL:  //查询直播信息及拉流URL
	             {
					 json liveinfoObj = m_object.at("live_info");
					 liveinfo = liveinfoObj.value("liveFlag", "oops");
					 pullUrl = liveinfoObj.value("pullUrl", "oops");		
				         LOG(INFO)<<"返回http 直播信息查询  liveFlag:"<<liveinfo <<"  pullUrl:"<<pullUrl<<"  直播ID:"<<m_recordID;
					 break;
				 }
				 case URL_TYPE::SELECT_LIVFLAG:  //查询直播状态
	             {
					 json liveinfoObj = m_object.at("live_info");
					 liveinfo = liveinfoObj.value("liveFlag", "oops");
                                         liveFlagTime = liveinfoObj.value("liveFlagTime", "oops");
                                         currentTimestamp = m_object.value("timestamp", "oops");
					 break;
				 }
			 }
         }else
         {  
            resCodeInfo = m_object.value("msg", "oops");
            LOG(ERROR)<<" Http接口返回异常  ret:"<<main_ret<<"  resCodeInfo:"<<resCodeInfo<<"  直播ID:"<<m_recordID;            
         }
       }else
       {
          LOG(ERROR)<<" Http接口返回数据不全 直播ID:"<<m_recordID;
          main_ret = 1;        
       }
    }else
    {
        LOG(ERROR) << "Http接口返回数据为空  直播ID:"<<m_recordID;
        main_ret = 2;
    }
	return main_ret;
}

//启动录制任务
int RecordSaveRunnable::StartRecord()
{
      //查询直播信息及拉流URL
      std::string resData ,resCodeInfo, liveinfo;
      string urlStr = IpPort;

      urlStr.append(liveSelect).append("?liveId=").append(m_recordID);
      urlStr.append("&userType=1");
      urlStr.append("&operateId=123");

      LOG(ERROR)<<"select LiveInfoUrl:"<<urlStr<<"  直播ID:"<<m_recordID;
      int m_ret = recive_http->HttpGetData(urlStr.c_str());

      if(0 == m_ret)
      {
         resData = recive_http->GetResdata();
         if(0 != ParseJsonInfo(resData ,resCodeInfo ,liveinfo ,m_pullUrl ,URL_TYPE::SELECT_LIVEURL))
         {
            LOG(ERROR) << "获取直播信息失败   ret:"<<m_ret<<" 直播ID:"<<m_recordID;
            m_ret = 1;
            return m_ret;
         }
      }else
      {              
         LOG(ERROR) << "调用直播信息查询接口失败:"<<m_ret<<" 直播ID:"<<m_recordID;
         return m_ret;
      }

      //新建文件,同时把直播信息写入json文件
      int ret = CreateFile();  

      if(0 != ret)
      {
         LOG(ERROR) << "新建文件失败  ret:"<<ret<<"  直播ID:"<<m_recordID;
         return ret;
      }

      ret = pthread_create(&producter_t, NULL, Recive_fun, (void *)this);
      if(0 != ret)
      {
         LOG(ERROR) << "创建读线程失败  ret:"<<ret<<"   直播ID:"<<m_recordID;
         return ret;
      }

      ret = pthread_create(&consumer_t, NULL, Save_fun,  (void *)this);
      if(0 != ret)
      {
      	  runningp = 0;
      	  pthread_join(producter_t,NULL);
          LOG(ERROR) << "创建写线程失败  ret:"<<ret<<"  直播ID:"<<m_recordID;
          return ret;
      }

      LOG(INFO) << "已启动录制读写线程  直播ID:"<<m_recordID;
       
      return ret;
} 

//结束录制任务
int RecordSaveRunnable::StopRecord()
{
      //runningp = 0;

      int resCode = pthread_join(producter_t,NULL);

      if(0 != resCode)
      {
          LOG(ERROR) << "销毁读线程失败  ret:"<<resCode<<"  直播ID:"<<m_recordID;
          return resCode;
      }
      printf("已经销毁读线程\n");

      resCode = pthread_join(consumer_t,NULL);

      if(0 != resCode)
      {
          LOG(ERROR) << "销毁写线程失败  ret:"<<resCode<<" 直播ID:"<<m_recordID;
          return resCode;
      }
      printf("已经销毁写线程\n");
	    
      if(NULL != afile)
      {
         fclose(afile);
         afile = NULL;
      }
	  
      if(NULL != vfile)
      {
        fclose(vfile);
        vfile = NULL;
      }

      if(NULL != wfile)
      {
         fclose(wfile);
         wfile = NULL;
      } 
      if(NULL != flvfile)
      {
         fclose(flvfile);
         flvfile = NULL;
      }
	  
      LOG(INFO) << "录制对象读 写线程都已停止  ret:"<<resCode<<" 直播ID:"<<m_recordID;

      return resCode;
}

//检测文件夹是否存在
int RecordSaveRunnable::CreateFileDir(const char *sPathName)  
{  
    char DirName[256];  
    strcpy(DirName, sPathName);  
    int i,len = strlen(DirName);
    for(i=1; i<len; i++)  
    {  
        if(DirName[i]=='/')  
        {  
            DirName[i] = 0; 
            if(access(DirName, F_OK)!=0)  
            {                     
              if(mkdir(DirName, 0755)==-1)  
              {   
		  LOG(ERROR) << "创建录制文件夹失败  直播ID:"<<m_recordID;
                  return -1;   
              }       
            }  
            DirName[i] = '/';  
       }  
    }  
    return 0;  
} 

//新建并打开文件
int RecordSaveRunnable::CreateFile()
{
     int ret = 0;
     int i = 0;
     char aFileStr[1024] = {0};
     char vFileStr[1024] = {0};
     char wFileStr[1024] ={0};
     char flvStr[1024] ={0};
 
     //file dir
     char fileDir[1024] = {0};  
 
     sprintf(fileDir ,"%s%s%s",FILEFOLDER.c_str(), m_recordID.c_str(),"/");

     ret = CreateFileDir(fileDir);
     if(0 != ret)
     {
       LOG(ERROR)<<" 创建file 文件夹失败  直播ID:"<<m_recordID;
       return ret;   
     }
  
     sprintf(aFileStr ,"%s%s%s",fileDir, m_recordID.c_str(), ".aac");

     sprintf(vFileStr ,"%s%s%s",fileDir, m_recordID.c_str(), ".h264");

     sprintf(wFileStr ,"%s%s%s",fileDir, m_recordID.c_str(), ".json");

     sprintf(flvStr ,"%s%s%s",fileDir, m_recordID.c_str(), ".flv");
  
     while((access(aFileStr, F_OK)) != -1)
     {
         i++;
         memset(aFileStr,0,1024);
         sprintf(aFileStr,"%s%s%s%d%s",fileDir,m_recordID.c_str(),"(+",i,").aac");

         memset(vFileStr,0,1024);
         sprintf(vFileStr,"%s%s%s%d%s",fileDir,m_recordID.c_str(),"(+",i,").h264");

         memset(wFileStr,0,1024);
         sprintf(wFileStr,"%s%s%s%d%s",fileDir,m_recordID.c_str(),"(+",i,").json");


         memset(flvStr,0,1024);
         sprintf(flvStr,"%s%s%s%d%s",fileDir,m_recordID.c_str(),"(+",i,").flv");
     }

     aacfileName = aFileStr;
     h264fileName = vFileStr;
     whitefileName = wFileStr;
     flvfileName = flvStr;

     //printf("文件名: %s %s %s \n", aFileStr, vFileStr, wFileStr);
     LOG(INFO) << "文件名: "<<aFileStr<<"  "<<vFileStr<<"  "<<wFileStr<<"  "<<flvStr;

     
     afile = fopen(aFileStr,"ab+");
     if(NULL == afile)
     {
        ret = errno;
        LOG(ERROR)<<" 打开aac文件失败  ret:"<<ret<<"  reason:"<<strerror(ret)<<"  直播ID:"<<m_recordID;
        return ret;
     }

    vfile = fopen(vFileStr,"ab+");
    if(NULL == vfile)
    {
       ret = errno;
       LOG(ERROR)<<" 打开h264文件失败  ret:"<<ret<<"  reason:"<<strerror(ret)<<"  直播ID:"<<m_recordID;;
       return ret;
    }

    wfile = fopen(wFileStr,"ab+");
    if(NULL == wfile)
    {
       ret = errno;
       LOG(ERROR)<<" 打开json文件失败  ret:"<<ret<<"  reason:"<<strerror(ret)<<"  直播ID:"<<m_recordID;;
       return ret;
    }

    /*if(!resData.empty())
    {
       fwrite(resData.c_str(), 1, resData.size(), wfile);
    }*/

    flvfile = fopen(flvStr,"ab+");
    if(NULL == flvfile)
    {
       ret = errno;
       LOG(ERROR)<<" 打开flv文件失败  ret:"<<ret<<"  reason:"<<strerror(ret)<<"  直播ID:"<<m_recordID;;
       return ret;
    }
   
    LOG(INFO) << "打开所有文件成功  直播ID:"<<m_recordID;
    return ret;
}

//读线程静态函数
void  *RecordSaveRunnable::Recive_fun(void* arg)
{
    return static_cast<RecordSaveRunnable*>(arg)->rtmpRecive_f();
}

//重连rtmp初始化
int RecordSaveRunnable::BrokenlineReconnectionInit(RTMP *m_pRtmp ,int reConntion)
{
     if(RTMP_IsConnected(m_pRtmp))
     {
         RTMP_Close(m_pRtmp);
     }                 
     if(NULL != m_pRtmp)
     {
         RTMP_Free(m_pRtmp);
         m_pRtmp = NULL;
     }
	 
     aacTagNum = 0;

     firstflag = true;  
  
     int ret = CreateFile();

     if(0 != ret)
     {
        LOG(ERROR) << "新建文件失败  ret:"<<ret<<"  直播ID:"<<m_recordID;
        return ret;
     }  
     return ret;
}


//重连rtmp,如小于三次,返回0;否则返回1
int RecordSaveRunnable::BrokenlineReconnection(int re_Connects)
{
	int ret = 0;
	if(re_Connects < 4)
	{
	      LOG(ERROR) << "准备重连 直播ID:"<<m_recordID;  

              //检测录制的音频 视频文件大小如果为0，删除录制的文件
              fseek(afile,0L,SEEK_END); /* 定位到文件末尾 */
              int aacDataLen=ftell(afile); /*得到文件大小*/
		 
	      fseek(vfile,0L,SEEK_END); /* 定位到文件末尾 */
              int h264DataLen=ftell(vfile); /*得到文件大小*/
		  
	      fclose(afile);
              fclose(vfile);
              fclose(wfile);
              fclose(flvfile);
		 
	      if(aacDataLen == 0 || h264DataLen == 0 || re_Connects > 1)
	      {
                   LOG(ERROR) << "上次录制的数据 AAC:"<< aacDataLen<<"  h264:"<< h264DataLen <<"  liveId:"<<m_recordID;
			
                   //识别到录制空文件，删除文件
                   remove(aacfileName.c_str());
                   remove(h264fileName.c_str());
                   remove(whitefileName.c_str());
                   remove(flvfileName.c_str());
	     }  
		
             //重连初始化
             ret = BrokenlineReconnectionInit(m_pRtmp,re_Connects);
		
	}else
	{
	   LOG(ERROR) << "重连次数超过三次，停止录制任务 直播ID:"<<m_recordID;

           //重连失败，删除空文件
           remove(aacfileName.c_str());
           remove(h264fileName.c_str());
           remove(whitefileName.c_str());
           remove(flvfileName.c_str());
                      
           save_httpflag = false;
           runningp = 0;
       
	   //上传录制状态
           m_ret = UpdataRecordflag(recive_http, RECORD_FLAG::RECORD_CONNECT_SERVER_ERROR);
	
	   ret = 1;
	}	
	return ret;
}

//读线程函数
void *RecordSaveRunnable::rtmpRecive_f()
{
    LOG(INFO) << "开始解析 rtmp: ";

    std::string resCodeInfo ,liveinfo ,urlparm;

    int bufsize = 1024 * 1024 * 10; 
    buf = (char*)malloc(sizeof(char) * bufsize);
    if(NULL == buf)
    {
       LOG(ERROR) << "RtmpInit no free memory  直播ID:"<<m_recordID ;
       return  (void*)0;
    }
    memset(buf, 0, bufsize);
	
    int re_Connects = 0;  //rtmp重连次数
    double duration = 0.0;
    uint32_t bufferTime = (uint32_t)(duration * 1000.0) + 5000; 

    m_pullUrl.append(m_recordID);
    //m_pullUrl = "rtmp://192.168.1.207/live/liveid666";

    recive_httpflag = 0;
    int m_ret = UpdataRecordflag(recive_http,RECORD_FLAG::RECORD_START);
 
begin: 

     m_pRtmp = RTMP_Alloc();    
     RTMP_Init(m_pRtmp);
     m_pRtmp->Link.timeout = 30;
   
     LOG(INFO) << "开始解析录制  rtmp:"<<m_pullUrl<<"  直播ID:"<<m_recordID;

     if(!RTMP_SetupURL(m_pRtmp,(char*)m_pullUrl.c_str()))
     {
         LOG(ERROR) << "拉流地址设置失败！ "<<m_pullUrl<<"  直播ID:"<<m_recordID;     			 
         m_ret = 3;
         goto end;
     }
     
     m_pRtmp->Link.lFlags |= RTMP_LF_LIVE; 
     RTMP_SetBufferMS(m_pRtmp, bufferTime);

     if(!RTMP_Connect(m_pRtmp, NULL))
     {
         LOG(ERROR)<< "RTMP服务连接失败 "<<"  直播ID:"<<m_recordID;        	 
         m_ret = 4;
         goto end;
     }
   
     if(!RTMP_ConnectStream(m_pRtmp, 0))
     {	
         LOG(ERROR) << "拉流连接失败  直播ID:"<<m_recordID;		 
         m_ret = 5;
         goto end;
     } 
     LOG(INFO) << "录制开始  直播ID:"<<m_recordID;
     while(runningp) 
     {	 
       int nRead = RTMP_Read(m_pRtmp, buf, bufsize);   

       fwrite(buf, sizeof(char), nRead, flvfile); 
       if(nRead > 0) //能读到数据
       {          
          if(0 != re_Connects)
          {
              re_Connects = 0;
          }
            
		  char *m_buf = buf; 
		  
          //第一次连RTMP时，除去FLV头
	  if(firstflag)
          {
            m_buf = m_buf+13;
	        nRead = nRead -13;
            firstflag= false;
          }
		   
          //写入缓冲区		   
          m_ret = m_cycleBuffer->write(m_buf,nRead);
		  
          if(0 != m_ret) //返回值不为0,写入异常
          {
			 if(1 == m_ret)
		     {
			    LOG(ERROR) << " Rtmp数据缓冲区空间使用率已大于0.5  直播ID:"<<m_recordID; 
				
		     }else if(2 == m_ret)
		     {
			    LOG(ERROR) << " Rtmp数据缓冲区空间不足  "<<nRead<<"字节未写入 直播ID:"<< m_recordID;
		     }  
	  }	 
     }else 
     {  

         //rtmp读不到数据,且收到停止录制命令,停止读取数据
        if(stopflag)
        {
                LOG(ERROR) << "rtmp读不到数据,且收到录制停止命令 直播ID:"<<m_recordID;
                runningp = 0;
                break;
        }
        //rtmp读数据超时
        if(RTMP_IsTimedout(m_pRtmp))
        {   	     
            //直播查询,看直播是否中断         
            string urlStr =  IpPort;
            urlparm = liveSelect;
            urlparm = urlparm.append("?liveId=").append(m_recordID);
            urlStr.append(urlparm);
            urlStr.append("&selectFlag=0");

            m_ret = recive_http->HttpGetData(urlStr.c_str());
            
            if(0 == m_ret) //调用直播查询接口成功
            { 
                 //获取解析查询返回值
                 std::string resData = recive_http->GetResdata();
		 std::string url;
                 m_ret = ParseJsonInfo(resData,resCodeInfo,liveinfo,url,URL_TYPE::SELECT_LIVFLAG);
      
                 int liveFlag = atoi(liveinfo.c_str());
                 LOG(INFO) <<"查询直播   直播状态:"<<liveinfo<< "  ret:"<<m_ret<<"   liveFlag:"<<liveFlag<<"  直播ID:"<<m_recordID;
                          
                 if(LIVE_FLAG::LIVE_START == liveFlag || LIVE_FLAG::LIVE_UPDATE == liveFlag)  //查询到还在直播中1或3
                 {
                        //获取liveflag更新的时间戳 
                        int FlagTime = atoi(liveFlagTime.c_str());
		        int CurrentTime = atoi(currentTimestamp.c_str());
					 
                        LOG(ERROR)<<"查询到的时间戳 liveFlagTime:"<<FlagTime<<"  currentTimestamp:"<<CurrentTime<<"  直播ID:"<<m_recordID;
					 
		         //liveflag已经超过60s未更新,默认客户端掉线，停止录制任务
		        if(CurrentTime - FlagTime > 60)
		        {
			       save_httpflag = false;    
                               runningp = 0;
						
			       //默认为客户端推流掉线，上传录制状态
                               m_ret = UpdataRecordflag(recive_http,RECORD_FLAG::RECORD_CLIENT_ERROR);
					
			       LOG(ERROR) <<"根据时间戳，检测到到直播停止或直播中断  直播ID:"<<m_recordID;
                               break;
	                }
                            re_Connects++;		 
		            m_ret = BrokenlineReconnection(re_Connects);
		            if(0 == m_ret)
		            {					
                                goto begin; //开始重连 			
		            }else
		            {
                                break;	//重连超过三次,停止录制任务					 
		            }    
                }else if(LIVE_FLAG::LIVE_STOP == liveFlag) //查询到已经停止直播
                {
			   save_httpflag = false;    
                           runningp = 0;
                 
			   LOG(ERROR) <<"查询到直播停止 liveFlag: "<<liveFlag <<"  直播ID:"<<m_recordID;
                           m_ret = UpdataRecordflag(recive_http,RECORD_FLAG::RECORD_STOP);
			   break;
	        }else if(LIVE_FLAG::LIVE_CLIENT_ERROR == liveFlag)  //查询到客户端推流异常
               {
		           save_httpflag = false;    
                           runningp = 0;
                 
			   LOG(ERROR) <<"查询到直播停止或直播中断 liveFlag: "<<liveFlag<<"  直播ID:"<<m_recordID;
                           m_ret = UpdataRecordflag(recive_http,RECORD_FLAG::RECORD_CLIENT_ERROR);	
                           break;

	       }else if(LIVE_FLAG::LIVE_SERVER_ERROR == liveFlag || LIVE_FLAG::LIVE_INIT == liveFlag) //查询到服务端拉流异常或为初始值
               {                
                           save_httpflag = false;    
                           runningp = 0;
		           LOG(ERROR) <<"查询到录制服务拉流异常 liveFlag:"<<liveFlag<<"  直播ID:"<<m_recordID;
                           break;
           			
               }else if(LIVE_FLAG::LIVE_TIME_OUT == liveFlag) //查询到客户端直播超时
               {
		          save_httpflag = false;    
                          runningp = 0;
                 
		          LOG(ERROR) <<"查询到直播停止或直播中断 liveFlag: "<<liveFlag<<"  直播ID:"<<m_recordID;
                          m_ret = UpdataRecordflag(recive_http,RECORD_FLAG::RECORD_CLIENT_TIMEOUT);			
                          break;	   
	      }else  //检测到其他异常
              {
                         save_httpflag = false;    
                         runningp = 0;			   
		         LOG(ERROR) <<"检测到其他异常  liveFlag:"<<liveFlag<<"  直播ID:"<<m_recordID;
		         break;
              }
	  	            
           }else  //调用直播查询接口失败
           {
                   LOG(ERROR) << "直播查询失败 "<<"错误代号:"<<m_ret<<"  直播ID:"<<m_recordID;
                       
                   //rtmp重连
                   re_Connects++;
                   m_ret = BrokenlineReconnection(re_Connects);
				   if(0 == m_ret)
			       {					
                       goto begin; //开始重连 		   
				   }else
				   {
                       break;	//重连超过三次,停止录制任务					 
				   }    
           }    
      }else   
      {
           //rtmp连接断开
           if(!RTMP_IsConnected(m_pRtmp))
           {
                LOG(ERROR) <<"直播过程中rtmp连接中断 进行重连 直播ID:"<<m_recordID;         
                //rtmp重连
                re_Connects++;			
                m_ret = BrokenlineReconnection(re_Connects);
				if(0 == m_ret)
			    {					
                    goto begin; //开始重连 			
				}else
				{
                    break;	//重连超过三次,停止录制任务						 
				}  
          }
       }
     } 
  }   
     
end:

   if(RTMP_IsConnected(m_pRtmp))
   {
	   RTMP_Close(m_pRtmp);
   }
   if(NULL != m_pRtmp)
   {
      RTMP_Free(m_pRtmp);
	  m_pRtmp = NULL;
   }
   if(runningp)
   {
      LOG(INFO) << "RTMP连接准备重连!";
	  re_Connects++;
	  if(re_Connects > 4) //重连三次失败，结束拉流
	  {
		 LOG(ERROR) << "三次都没有连上Rtmp服务  直播ID:"<<m_recordID; 
		 //上传录制状态,连不上rtmp服务
		 BrokenlineReconnection(re_Connects);		 
	  }else
	  {
		  goto begin;
	  }	 
  }
  if(NULL != buf)
  {
	 free(buf);
	 buf = NULL;
  }

  //设置停止录制标志
  stopRecordflag = true;

  LOG(INFO) << "读线程结束  直播ID: "<<m_recordID;
  return  (void*)0;
}

//写线程静态函数
void *RecordSaveRunnable::Save_fun(void *arg)
{
    return static_cast<RecordSaveRunnable*>(arg)->rtmpSave_f();
}

//写线程函数
void *RecordSaveRunnable::rtmpSave_f()
{
	
	//Tag头缓冲区
	int tagHeadSize = 11;
	char *tagHead_buf = (char*)malloc(tagHeadSize);
	if(NULL == tagHead_buf)
        {
           LOG(ERROR) << "tagHeadSize 申请内存失败  直播ID:"<<m_recordID;
           return  (void*)0;
        }
	memset(tagHead_buf, 0 ,tagHeadSize);
	
    //Tag数据缓冲区
    int TAG_BUFF_SIZE = 5 * 1024 *1024;
    int tagdataSize = 0;
    char *tagData_buf  = (char*)malloc(TAG_BUFF_SIZE);
    if(NULL == tagData_buf)
    {
       LOG(ERROR) << "tagData_buf 申请内存失败  直播ID:"<<m_recordID;
       return  (void*)0;
    }
    memset(tagData_buf, 0 ,TAG_BUFF_SIZE);
	
    //Tag数据长度缓冲区
    int tagSize = 4;
	
	int m_ret = 0;
    bool tagFlag = true;
	int ToRead = 0;
	int readTagSize = 0;

    while(runningc)
    {	              	
        if(tagFlag) //开始解析Tag头
        {	
            ToRead = tagHeadSize;		
		    m_ret = m_cycleBuffer->read(tagHead_buf,ToRead);
		      
		    if(0 != m_ret) //未读取到Tag头
		    {
			   if(stopRecordflag) //读线程已经结束，剩余数据不足一个Tag头
			   {
				  LOG(INFO) << "写缓存结束  直播ID: "<<m_recordID;				  
				  break;		  
			   }
                           //LOG(INFO) << "写缓存死循环1111  直播ID: "<<m_recordID;
			   continue;	  
		   }
		 
		   //获取数据长度
		   memcpy(&tagdataSize, tagHead_buf + 1, 3);
                   tagdataSize = HTON24(tagdataSize);
           		  
	           //tagdataSize大于TAG_BUFF_SIZE时 重新申请为原来的两倍内存
	           if(tagdataSize + tagSize  >  TAG_BUFF_SIZE)
	           {
		        if(NULL != tagData_buf)
		        {
			    free(tagHead_buf);
		            tagHead_buf = NULL;
		        }				 		  
		        TAG_BUFF_SIZE += TAG_BUFF_SIZE;
		        tagData_buf = (char*)malloc(TAG_BUFF_SIZE);
		        if(NULL == tagData_buf)
                {
                     LOG(ERROR) << "tagData_buf 申请内存失败  直播ID："<<m_recordID;
		             break;
                }
		        memset(tagData_buf, 0 ,TAG_BUFF_SIZE);
		   }
			 
           tagFlag = false;             
         
		}else //开始解析帧数据
	    {   
          
		   ToRead = tagdataSize + tagSize;
	       m_ret = m_cycleBuffer->read(tagData_buf,ToRead);  //去读取Tag数据 + 4字节TagSize
		   
		   if(0 != m_ret) //未读取到Tag数据
		   {
			  if(stopRecordflag) //读线程已经结束,剩余数据不足一个TagData
			  {
				  LOG(INFO) << "写缓存结束  直播ID: "<<m_recordID;				  
				  break;		  
			  }
                          LOG(INFO) << "写缓存死循环2222  直播ID: "<<m_recordID;
			  continue;
		   }	    
		    
		  //解析四字节的TagSize长度
		  memcpy(&readTagSize, tagData_buf + tagdataSize, tagSize);
                  readTagSize = HTON32(readTagSize);
  
		  if(readTagSize == 11 + tagdataSize)
	      {
			  m_ret = WriteFile(tagHead_buf, tagData_buf, tagdataSize);
			  if(m_ret != 0)
			  {
				 LOG(ERROR) << "写入tag长度失败共:  "<< tagdataSize<<"字节未写入 直播ID:"<<m_recordID;   
			  }
		  }else
		  {
			 LOG(ERROR) << "解析tag数据失败共:  "<< tagdataSize<<"字节未写入 直播ID:"<<m_recordID; 
		  }		  
		/*   m_ret =  WriteFile(tagHead_buf, tagData_buf, tagdataSize);
				
	      if(m_ret != 0)
		  {
			  LOG(ERROR) << "写入tag长度失败共:  "<< tagdataSize<<"字节未写入 直播ID:"<<m_recordID;
		  } */
	  	  		 		  
          //重置Tag头flag				
	      tagFlag = true;	
	   }
    }
	
	LOG(INFO) << "写线程结束 直播ID:"<<m_recordID;
	
	//上传录制完成状态
	UploadRecordStopFlag();
	
    if(NULL != tagHead_buf)
	{
		free(tagHead_buf);
		tagHead_buf = NULL;
	}	
	if(NULL != tagData_buf)
	{
		free(tagData_buf);
		tagData_buf = NULL;
	}
    return  (void*)0;    
}

//写线程结束上传录制完成状态
void RecordSaveRunnable::UploadRecordStopFlag()
{
    if(save_httpflag) //录制为正常结束，上传录制完成状态
    {
        m_ret = UpdataRecordflag(save_http,RECORD_FLAG::RECORD_STOP);
	if(0 != m_ret)
        {
         LOG(ERROR) << "调用上传录制完成状态接口失败   m_ret:"<<m_ret<<" 直播ID"<<m_recordID;      
        }
    }else  //录制异常结束,删除队列中录制任务
    {
		
        std::string UrlStr = "http://localhost:";
        UrlStr.append(ServerPort);
        UrlStr.append(APIStr);
        UrlStr.append("?liveId=");
        UrlStr.append(m_recordID);
        UrlStr.append("&type=1");

        printf("delete url: %s\n", UrlStr.c_str());
        m_ret = save_http->HttpGetData(UrlStr.c_str()); 
	if(0 != m_ret)
        {
           LOG(ERROR) << "调用删除录制任务接口失败   m_ret:"<<m_ret<<" 直播ID"<<m_recordID;      
        }
   }		
}

//上传白板数据
int RecordSaveRunnable::UploadWhiteData(LibcurClient *httpclient, std::string data)
{
      string urlStr = IpPort;
      
      urlStr.append(liveUpload);

      printf("uploadURL: %s\n", urlStr.c_str());
      
      //int m_ret = httpclient->HttpPostData(urlStr.c_str(), m_recordID ,data);
      if(0 != m_ret)
      {
         LOG(ERROR) << "调用上传白板数据接口失败  m_ret:"<<m_ret<<" 直播ID"<<m_recordID;      
      }
      return m_ret;
}

//更新录制状态
int RecordSaveRunnable::UpdataRecordflag(LibcurClient *http_client ,int flag)
{   
    std::string  resCodeInfo ,liveinfo ,pullUrl, url;      
    std::string urlparm = liveUpdate;
    urlparm = urlparm.append("?liveId=");
   
    urlparm.append(m_recordID);
    urlparm.append("&recordFlag=");
    //urlparm.append("&operateId=123");

    char flagStr[10] ={};
    snprintf(flagStr, sizeof(flagStr), "%d",flag);
    urlparm.append(flagStr);
    urlparm.append("&operateId=123");

    std::string updataUrl = IpPort + urlparm;
 
    int m_ret = http_client->HttpGetData(updataUrl.c_str());
	
    if(0 != m_ret)
    {
        LOG(ERROR)<<"调用更新录制状态接口失败  直播ID"<<m_recordID;
	return m_ret;
    }
    std::string resData = http_client->GetResdata();

    LOG(INFO)<<"上传录制状态  resData:"<<resData<<" flag:"<<flag;
	
    m_ret = ParseJsonInfo(resData,resCodeInfo,liveinfo,url,URL_TYPE::UPDATA_RECORDFLAG);

    return m_ret;
}

//获取直播流tag数据进行写文件
int RecordSaveRunnable::WriteFile(char *tagHead_buf,  char *tagData_buf, int tagdataSize)
{          
         
    //时间戳指针
    char *timestamp_buf = tagHead_buf + 4;
	
    if(tagHead_buf[0] == 0x09) // 视频 
    {    
	   if(0 != Write264data(timestamp_buf ,tagData_buf,tagdataSize))
       {
           LOG(ERROR) << "视频tag写入失败 直播ID: "<<m_recordID;
           return 1;
       }
	   
    }else if(tagHead_buf[0] == 0x08) //音频
    {   
	   if(0 != WriteAac(timestamp_buf ,tagData_buf,tagdataSize))
       {
          LOG(ERROR) << "音频tag写入失败 直播ID: "<<m_recordID;
          return 2;
       }
             
    }else if(tagHead_buf[0] == 0x12) //白板
    {
       bool ok = WriteExtractDefine(timestamp_buf ,tagData_buf,tagdataSize);
       if(!ok)
       {
          LOG(ERROR) << "白板tag写入失败 直播ID: "<<m_recordID;
          return 3;
       }
    } 
    return 0;     
}

//白板数据写入json文件
bool RecordSaveRunnable::WriteExtractDefine(char *timebuff, char *data, int tagdataSize)
{

     bool bResult = true;
     int iLenVale = 0;
     std::string strDefine;
     if (0x02 != *data || 0x05 != *(data + 2) || 0 != memcmp(data + 3, "onUDD", 5) || 0x08 != *(data + 8))
     {
       printf("%s\n","fail");
       bResult = false;
       return bResult;
     }
     int iArrayNum = 0;
     memcpy(&iArrayNum, data + 9, 4);
     iArrayNum = HTON32(iArrayNum);
     char* pTmp = data + 13;
     for(int i = 0; i < iArrayNum; i++)
     {
       int iLenKey = 0;
       memcpy(&iLenKey, pTmp, 2);
       iLenKey = HTON16(iLenKey);
       pTmp += 2;
       pTmp += iLenKey;
       if(0x02 == *pTmp)//数据类型
       {
          pTmp++;
          //int iLenVale = 0;
          memcpy(&iLenVale, pTmp, 2);
          iLenVale = HTON16(iLenVale);
          pTmp += 2;
          strDefine.append((char*)pTmp, iLenVale);
       }
    }

    printf("shuju:  %d  直播ID:%s\n",iLenVale ,m_recordID.c_str());

	//上传白板数据
    //UploadWhiteData(upload_http, strDefine);
	
	//写入时间戳
    // if(timestampSize != fwrite(timebuff, sizeof(char), timestampSize ,wfile))
    // {
    //    bResult = false;
	//    return bResult;
    // }

    if(iLenVale != fwrite(strDefine.c_str(), 1, iLenVale, wfile))
    {
       bResult = false;
	   return bResult;
    }

    return bResult;
}


//aac数据写入aac文件
int RecordSaveRunnable::WriteAac(char *timebuff ,char *data, int datasize)
{       
	if(data[1] == 0x00)
	{
		iHasAudioSpecificConfig = 1;
		memcpy(&ah, data+2, sizeof(AdtsHeader));
		ad.check1 = 0xff;
		ad.check2 = 0xf; //0xff
		ad.check3 = 0x1f; //0xff
		ad.check4 = 0x3f; //0xff
		ad.protection = 1;
		ad.ObjectType = 0;
		ad.SamplingIndex = ah.SamplIndex2 | ah.SamplIndex1 << 1;
		ad.channel2 = ah.channel;
	}else
	{
                
		if(iHasAudioSpecificConfig)
		{
			unsigned int size = datasize - 2 + 7 + 4;  //7个字节ADTS头和四字节的时间戳
			ad.length1 = (size >> 11) & 0x03;
			ad.length2 = (size >> 3) & 0xff;
			ad.length3 = size & 0x07;

			if (sizeof(AdtsData) != fwrite((char*)&ad, 1, sizeof(AdtsData), afile))
			{      
			 	return 1;
		    }
		}
     
        //写入时间戳
        if(timestampSize != fwrite(timebuff, 1, timestampSize, afile))
        {
           return 2;
        }
 
      if(datasize - 2 != fwrite(data + 2, 1, datasize - 2, afile))
	  {    
	  	 return 3;
	  }
   }

   aacTagNum++;
   if(aacTagNum == 200)
   {
      //调用直播状态接口,上传状态录制中
      if(save_httpflag)
      {
         recive_httpflag = 1;
         int m_ret = UpdataRecordflag(save_http,RECORD_FLAG::RECORD_UPDATE);
      } 
      aacTagNum = 0;
   }
   return 0;
}


//264数据写入.h264文件
int RecordSaveRunnable::Write264data(char *timebuff, char *packetBody, int datasize)
{      
     char flag[] = {0x00,0x00,0x00,0x01};
     char *p = packetBody;

     char sizebuff[4] = {0};    
     int len = 0;
     int buffSize = 0;
  
     if(((packetBody[0] & 0x0f) == 7)&& ((packetBody[1] & 0x0f) == 0)) 
     {  

   	     p = p + 11;
		 
		 //获取sps数据长度
	     char sps[2]= {0};
	     strncpy(sps,p,2);
		 
	     char *s = (char*)&len;
	     *(s+1) = *(p);
	     *(s) = *(p+1);
       
         //写入sps数据
  	 p = p + 2; 
	 
         /*if(4 != fwrite(flag, sizeof(char), 4, vfile))
         {
             return 1;
         }*/

          //写入长度
         buffSize = len + 4;
	 sizebuff[0] = (buffSize & 0xff000000) >> 24;  
         sizebuff[1] = (buffSize & 0x00ff0000) >> 16;  
         sizebuff[2] = (buffSize & 0x0000ff00) >> 8;  
         sizebuff[3] = (buffSize & 0x000000ff);  	
         if(4 != fwrite(sizebuff, sizeof(char), 4 ,vfile))
         {
             return 2;
         }

         //写入时间戳
         if(4 != fwrite(timebuff, sizeof(char), 4 ,vfile))
         {
             return 2;
         }

         if(len != fwrite(p, sizeof(char), len, vfile))
         {
             return 3;
         }
       
	     //获取pps数据长度
	     p = p +  len + 1; 
	     s = (char*)&len;
	     *(s+1) =*(p);
	     *(s) = *(p+1);
        
         //写入pps数据
	 p = p+2;

	 /*if(4 != fwrite(flag, sizeof(char), 4, vfile))
         {
            return 4;
         }*/

          //写入长度
	 buffSize = len + 4;
         memset(sizebuff,0 ,4);
         sizebuff[0] = (buffSize & 0xff000000) >> 24;  
         sizebuff[1] = (buffSize & 0x00ff0000) >> 16;  
         sizebuff[2] = (buffSize & 0x0000ff00) >> 8;  
         sizebuff[3] = (buffSize & 0x000000ff);  	
         if(4 != fwrite(sizebuff, sizeof(char), 4 ,vfile))
         {
             return 2;
         }

          //写入时间戳
         if(4 != fwrite(timebuff, sizeof(char), 4 ,vfile))
         {
             return 2;
         }

        if(len != fwrite(p, sizeof(char), len, vfile))
        {
            return 6;
        }	
     }else
     {
	 p = p + 9; 

	 /*if(4 != fwrite(flag, sizeof(char), 4, vfile))
         {
            return  7;
         }*/

         buffSize = datasize - 9 + 4;
         memset(sizebuff,0 ,4);
         sizebuff[0] = (buffSize & 0xff000000) >> 24;
         sizebuff[1] = (buffSize & 0x00ff0000) >> 16;
         sizebuff[2] = (buffSize & 0x0000ff00) >> 8;
         sizebuff[3] = (buffSize & 0x000000ff);
         if(4 != fwrite(sizebuff, sizeof(char), 4 ,vfile))
         {
             return 2;
         }
         //写入时间戳
         if(4 != fwrite(timebuff, sizeof(char), 4 ,vfile))
         {
             return 8;
         }

         if((datasize - 9) != fwrite(p, sizeof(char), datasize - 9, vfile))
         {
             return  9;
         }
    }
    return 0;
}
RecordSaveRunnable::~RecordSaveRunnable()
{  
      if(NULL != m_pRtmp)
      {
	  RTMP_Free(m_pRtmp);
	  m_pRtmp = NULL;
      }

      if (NULL != buf)
      {
	 free(buf);
         buf = NULL;
      }

      if(NULL != m_cycleBuffer)
      {
          delete m_cycleBuffer;
          m_cycleBuffer = NULL;
      }
  
     if(NULL != recive_http)
     {
        delete recive_http;
        recive_http = NULL;
     }

     if(NULL != save_http)
     {
        delete save_http;
        save_http = NULL;
     }

     if(NULL != upload_http)
     {
        delete upload_http;
        upload_http = NULL;
     }
}
