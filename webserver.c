#include <queue>
#include <map>
#include <list>

#include "mongoose.h"
#include "glog/logging.h"
#include "Httpclient/LibcurClient.h"
#include "RecordSave/RecordSaveRunnable.h"
#include "message_queue.h"
#include "json.hpp"

using json = nlohmann::json;

using namespace std;

std::queue<liveParmStruct*> liveParmQueue; //直播参数队列

std::map<std::string, RecordSaveRunnable*> RecordSaveMap; //直播对象队列

std::list<std::string> DeleteRecordList;    //定时删除录制任务的队列
 
static pthread_mutex_t record_mutex = PTHREAD_MUTEX_INITIALIZER;  //直播对象队列互斥量

static pthread_mutex_t delete_mutex = PTHREAD_MUTEX_INITIALIZER;  //定时删除录制队列互斥量

//信号量
sem_t bin_sem;
sem_t bin_blank;

//线程对象
pthread_t recordManage_t;
pthread_t httpServer_t;
pthread_t httpTime_t;
pthread_t checkDisk_t;
pthread_t deletRecordTask_t;

//http请求对象
LibcurClient *m_httpclient, *s_httpclient;

int parseResdata(string &resdata,  int ret ,PARSE_TYPE m_Type);
void setTimer(unsigned seconds ,TIMER_TYPE TimerFlag);
void *stopRecord_fun(void *data);
void updateOnline_fun();
void checkdisk_fun();
void deleteRecord_fun();


//http监听服务 线程
void ev_handler(struct mg_connection *nc, int ev, void *ev_data) 
{
   switch (ev) 
   {
	   
    case MG_EV_ACCEPT: 
	{
        char addr[32];
        mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
        LOG(INFO) << "Connection from:"<<nc<<addr;
        break;
    }
    case MG_EV_HTTP_REQUEST: 
    {
      struct http_message *hm = (struct http_message *) ev_data;
      char addr[32];
      mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr),
                          MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
      
	  //解析http请求方法名
	  char methodStr[(int)hm->uri.len];
      sprintf(methodStr, "%.*s",(int)hm->uri.len,hm->uri.p);
	  
      RESCODE ret = RESCODE::NO_ERROR;
	  
      if(strcmp(methodStr, "/live/record") == 0)
      {    
         LOG(INFO) << "开始解析:"<<methodStr;
		 
         //解析http请求参数
		 int parmlen = (int)hm->query_string.len;
         char parmStr[parmlen];
         sprintf(parmStr, "%.*s",parmlen,hm->query_string.p);
		 
		 printf("url参数长度：%d  %s\n",parmlen ,parmStr);
		     
         char *liveId_buf = (char*)malloc(sizeof(char)*parmlen);
         memset(liveId_buf, 0 ,sizeof(char)*parmlen);
         if(NULL == liveId_buf)
         {       
             LOG(ERROR) << "参数liveId malloc失败:"<<liveId_buf;
             ret = RESCODE::MALLOC_ERROR;
             goto end;            
         }

         char *type_buf = (char*)malloc(sizeof(char)*parmlen);
         memset(type_buf, 0 ,sizeof(char)*parmlen);
         if(NULL == type_buf)
         {       
             LOG(ERROR) << "参数liveType malloc失败:"<<type_buf;
             ret = RESCODE::MALLOC_ERROR;
             goto end;            
         }
		 
         mg_get_http_var(&hm->query_string, "liveId", liveId_buf, parmlen); //获取liveID	 
	     mg_get_http_var(&hm->query_string, "type", type_buf, parmlen);  //获取录制命令
		    
         LOG(INFO) << "获取参数 : "<<liveId_buf<<"  "<<type_buf;   

         //目前录制命令只支持0 1
         if((strcmp(type_buf,"0") == 0) || (strcmp(type_buf,"1") == 0))
         {
            if(strlen(liveId_buf) > 0)
            {
			   liveParmStruct *m_parmData = (liveParmStruct*)malloc(sizeof(liveParmStruct));
               m_parmData->liveID = liveId_buf;
			   m_parmData->liveType = type_buf;     

               sem_wait(&bin_blank);  			
               liveParmQueue.push(m_parmData);			
               sem_post(&bin_sem);                 
            }else
			{
				LOG(ERROR)<<"参数错误，直播ID为空";
                ret = RESCODE::LIVEID_ERROR;
			}
         }else
         {
            LOG(ERROR)<<"未知的录制命令  直播ID:"<<liveId_buf;
            ret = RESCODE::TYPE_ERROR;
         } 	
     }else
     {
        ret = RESCODE::METHOD_ERROR;
     }    
end:
      char numStr[10] ={};
      snprintf(numStr, sizeof(numStr), "%d",ret);

      //json返回值
      json obj;
      obj["code"] = numStr;
      std::string resStr = obj.dump();

      mg_send_response_line(nc, 200, "Content-Type: application/json;charset=utf-8\r\n");
      mg_printf(nc,"\r\n%s\r\n",resStr.c_str());
    
      nc->flags |= MG_F_SEND_AND_CLOSE;

      break;
      }
    case MG_EV_CLOSE: 
	{
      LOG(INFO) << "Connection closed:"<<nc;   
      break;
    }
  }
}

//录制任务管理 线程
void *recordManage_fun(void *data)
{
    int ret = 0;
    while(recordMange_flag)
    {      
        sem_wait(&bin_sem); 
	
        liveParmStruct *pdata = liveParmQueue.front();
                
        LOG(INFO) << "管理线程获取参数 直播ID:"<<pdata->liveID<<"  Type:"<<pdata->liveType;

        if((strcmp(pdata->liveType,"0")) == 0) //开始录制
        {      
			 pthread_mutex_lock(&record_mutex); 
			 
			 //判断liveID是否已经存在
             if(RecordSaveMap.find(pdata->liveID) == RecordSaveMap.end()) //录制对象队列不存在本ID,加入队列
			 {
				  //新建录制对象,并启动录制
				  RecordSaveRunnable *recordRun = new RecordSaveRunnable(pdata->liveID);   
                  ret = recordRun->StartRecord();
				  
				  if(ret == 0)//启动成功
                  {		  					  
					  RecordSaveMap.insert(std::pair<std::string, RecordSaveRunnable*>(pdata->liveID,recordRun));
        
                      pthread_mutex_unlock(&record_mutex);

                      LOG(INFO) << "启动录制任务成功   ret:"<<ret<<"   直播ID:"<<pdata->liveID;
                 }else
                 { 
                    pthread_mutex_unlock(&record_mutex);
    	            
                    if(NULL != recordRun)
                    {
	                    delete recordRun;
                        recordRun = NULL;
                    }
                    LOG(ERROR) << "启动录制任务失败  ret:"<<ret<<"   直播ID:"<<pdata->liveID;					
				 }					
             }else
             {
				   pthread_mutex_unlock(&record_mutex);
                   LOG(ERROR) << "该录制任务正在录制中  直播ID:"<<pdata->liveID;         
             }    
			 
       }else if(strcmp(pdata->liveType,"1") == 0)  //结束录制
       {
			pthread_mutex_lock(&record_mutex);
			
			std::map<std::string, RecordSaveRunnable*>::iterator iter = RecordSaveMap.find(pdata->liveID);
            			
            if(iter != RecordSaveMap.end()) //在录制对象队列中找到找到liveID,停止录制
		    {
				 RecordSaveRunnable *m_runnable = (iter)->second;
				 
                 pthread_t deleteRunnable_t;
                 ret = pthread_create(&deleteRunnable_t, NULL,stopRecord_fun, (void*)m_runnable);
				 pthread_mutex_unlock(&record_mutex); 
                 if(0 == ret)
                 {
					LOG(INFO) << "创建停止录制任务线程成功  ret:"<<ret<<"   直播ID:"<<pdata->liveID; 
                 }else
			     {
					LOG(ERROR) << "创建停止录制任务线程失败  ret:"<<ret<<"   直播ID:"<<pdata->liveID;
				 }		  		 
			}else
			{
				 pthread_mutex_unlock(&record_mutex);
                 LOG(ERROR) << "未找到该录制任务  直播ID:"<<pdata->liveID;    
			}
     }
		
     if(NULL != pdata->liveID)
     {
         free(pdata->liveID);
         pdata->liveID = NULL;
     }
     if(NULL != pdata->liveType)
     {
	    free(pdata->liveType);
        pdata->liveType = NULL;
     }
		
     liveParmQueue.pop();
       
     sem_post(&bin_blank); 
	 
   }
   return data;
}

//停止录制任务 线程
void *stopRecord_fun(void *data)
{
     RecordSaveRunnable *m_runnable  = (RecordSaveRunnable*)data;

	 int ret = m_runnable->StopRecord();
	 	 
	 if(0 == ret)
     {
         LOG(INFO)<< "停止录制任务成功 ret:"<<ret<<"   直播ID:"<<m_runnable->m_recordID;                
     }else
     {
         LOG(ERROR) << "停止录制任务失败 ret:"<<ret<<"   直播ID:"<<m_runnable->m_recordID;
     }
	 
	 pthread_mutex_lock(&delete_mutex); 
     DeleteRecordList.push_back(m_runnable->m_recordID);	  
	 pthread_mutex_unlock(&delete_mutex);
	    
     pthread_detach(pthread_self()); 

     return data;
}

//解析http返回json数据
int parseResdata(string &resdata,  int ret ,PARSE_TYPE m_Type)
{     
    if(!resdata.empty())
    {   
        json m_object = json::parse(resdata);
        if(m_object.is_object())
        {
            string resCode = m_object.value("code", "oops");
            ret = atoi(resCode.c_str() );
            if(0 == ret)
            {	  
		        switch(m_Type) 
                {
		        	case PARSE_TYPE::GETAPI:  //解析获取API接口
	                {
				        string IP = m_object.value("ip", "oops");
                        string port = m_object.value("port", "oops");
                        IpPort = IpPort.append(IP).append(":").append(port);

                        ServerCreate = m_object.value("server_create", "oops");
                        ServerDelete = m_object.value("server_delete", "oops");
                        ServerSelect = m_object.value("server_select", "oops");
                        //ServerUpdate = m_object.value("server_update", "oops");

                        liveUpdate = m_object.value("live_update", "oops");
                        liveSelect = m_object.value("live_select", "oops");
                        liveUpload = m_object.value("live_upload", "oops");

                        cout<<IpPort  <<ServerCreate  <<ServerDelete  <<ServerSelect  <<liveUpdate <<endl;	 
			            break;
	               }
			       case PARSE_TYPE::REGISTONLINE:  //解析注册录制服务
	               {
				       record_serverId = m_object.value("ServerID", "oops");
                       LOG(INFO)<<"录制服务 record_serverId:"<<record_serverId;		
			           break;
	               }
			       case PARSE_TYPE::UPDATA:  //解析定时上传录制在线
	               {
					   break;
	               }	    
              }			  
         }else
         {
			 LOG(ERROR)<<"接口返回数据异常 ret:"<< ret<<"  错误信息:"<<m_object.value("msg", "oops");
         }  
	  }else
      {
		  ret = -1;
          LOG(ERROR)<<" 接口返回数据不完整";
	  }		   
   }else
   {
	   ret = -2;
	   LOG(ERROR) << "Http接口返回 数据为空";   
   }
   
   return ret;
}

//设置定时器任务
void setTimer(unsigned seconds ,TIMER_TYPE TimerFlag)
{
    struct timeval tv;
    time_t tt;
    tv.tv_sec=seconds;
    tv.tv_usec=0;
    int err;
 do{
     err=select(0,NULL,NULL,NULL,&tv);
     time(&tt);
	 
	 switch (TimerFlag) 
     {
        case TIMER_TYPE::UPDATEONLINE:  //定时上传录制在线
	    {
			 updateOnline_fun();
			 break;
	    }
		case TIMER_TYPE::DELETERECORD: //定时遍历及删除已停止的录制任务
	    {
			 deleteRecord_fun();
			 break;
	    }
		case TIMER_TYPE::CHEDISK:  //定时检测磁盘空间
	    {
			 checkdisk_fun();
			 break;
	    }
	 }
  }while(err<0 && errno==EINTR);
}

//遍历及删除已停止的录制任务
void deleteRecord_fun()
{
	pthread_mutex_lock(&delete_mutex);
	printf("delete record...\n");
	std::list<std::string>::iterator iter;
	for(iter= DeleteRecordList.begin(); iter != DeleteRecordList.end();)
	{
	   std::string liveID = *iter;    
	   pthread_mutex_lock(&record_mutex);
	   std::map<std::string, RecordSaveRunnable*>::iterator it = RecordSaveMap.find(liveID);
			
       if(it != RecordSaveMap.end()) //在录制对象队列中找到liveID，删除录制对象
	   {
		  RecordSaveRunnable *m_runnable = (it)->second;
		  
		  //删除队列中的录制任务
		  if(NULL != m_runnable)
		  {
			  delete m_runnable;
			  m_runnable = NULL;
		  }
		  LOG(INFO)<<"删除录制任务成功  直播ID:"<< liveID;
		  RecordSaveMap.erase(it);
	   }
	        
	   pthread_mutex_unlock(&record_mutex);
	      
	   iter =  DeleteRecordList.erase(iter);     
	}
	
	if(!DeleteRecordList.empty())
	      DeleteRecordList.clear();
	  
    pthread_mutex_unlock(&delete_mutex); 
}

//检测磁盘空间
void checkdisk_fun()
{
    char path[1024] ;
 
    //获取当前的工作目录
    char *p = getcwd(path , 1024);
    printf("buffer:%s  p:%s size:%zu\n" , path , p , strlen(path));

    struct statfs diskInfo;
    statfs(path, &diskInfo);
    unsigned long long totalBlocks = diskInfo.f_bsize;
    unsigned long long totalSize = totalBlocks * diskInfo.f_blocks;
    size_t mbTotalsize = totalSize>>20;
    unsigned long long freeDisk = diskInfo.f_bfree*totalBlocks;
    size_t mbFreedisk = freeDisk>>20;
    printf ("%s: total=%lld total=%zuMB,free=%lld,  free=%zuMB\n",path, totalSize , mbTotalsize, freeDisk ,mbFreedisk);

    if(mbFreedisk < 2048)
    {
       LOG(WARNING)<<"磁盘空间即将不足 剩余空间:"<<mbFreedisk;
    }
}

//上传录制在线
void updateOnline_fun()
{
	int main_ret = s_httpclient->HttpGetData(updateOnlineUrl.c_str());
    if(0 == main_ret)
    {
		std::string resData = s_httpclient->GetResdata();
        main_ret = parseResdata(resData, main_ret ,PARSE_TYPE::UPDATA);
		if(0 != main_ret)
		{
		  LOG(ERROR) << "解析定时返回数据失败  main_ret:"<<main_ret; 
		}
    }else
    {
       //LOG(ERROR) << "调用定时上在线状态接口失败  main_ret:"<<main_ret;  
    }	
}

//定时删除已停止的录制任务 线程
void *deletRecord_fun(void *data)
{
   while(record_flag)
   {  
      setTimer(30, TIMER_TYPE::DELETERECORD);
   }
   return data;
}

//定时检测磁盘 线程
void *checkDisk_fun(void *data)
{
   while(record_flag)
   {  
      setTimer(60 * 30 , TIMER_TYPE::CHEDISK);
   }
   return data;
}

//定时上传录制在线 线程
void *httpTime_fun(void *pdata)
{
   s_httpclient = new LibcurClient;

   updateOnlineUrl = IpPort;

   updateOnlineUrl.append(ServerUpdate);
   updateOnlineUrl.append("serverId=");
   
   updateOnlineUrl.append(record_serverId);
   updateOnlineUrl.append("&netFlag=20");
   cout<<"time url:"<<updateOnlineUrl<<endl;

   //录制服务在线状态定时上传
   while(record_flag)
   {   
	  setTimer(5 , TIMER_TYPE::UPDATEONLINE);   
   }
   if(NULL != s_httpclient)
   {
      delete s_httpclient;
      s_httpclient = NULL;
   }
   return pdata;
}
//http服务监听 线程
void *httpServer_fun(void *pdata)
{
   printf("httpServer_fun :[tid: %ld]\n", syscall(SYS_gettid));
   struct mg_mgr mgr;
   struct mg_connection *nc;

   mg_mgr_init(&mgr, NULL);
   
   printf("启动Http服务 port:%s\n", s_http_port);
   LOG(INFO) << "Http服务启动  port:"<<s_http_port;
   nc = mg_bind(&mgr, s_http_port, ev_handler);
   if(nc == NULL) 
   {
      printf("Failed to create listener\n");
      LOG(ERROR) << "Http服务 监听端口失败";
      httpSev_flag = 0;
      pthread_detach(pthread_self());
      return 0;
   }

   mg_set_protocol_http_websocket(nc);

   while(httpSev_flag)
   {    
      mg_mgr_poll(&mgr, 1000);
   }

   return pdata;
 }
 
//创建日志文件夹
int CreateLogFileDir(const char *sPathName)  
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
                      printf("mkdir log error\n");
                      LOG(ERROR) << "创建日志文件夹失败";					  
                      return -1;   
                  }                   
              }  
              DirName[i] = '/';  
          }  
      }  
      return 0;  
}
//启动服务
int startServer(void)
{
    int main_ret = 0;
	std::string resStr = "";
    m_httpclient = new LibcurClient;
    std::string url;

    main_ret = CreateLogFileDir(LOGFOLDER.c_str());
    if(0 != main_ret)
    {
       LOG(ERROR) << "创建log 文件夹失败"<<"   main_ret:"<<main_ret;
       return main_ret ;
     }

	//创建log初始化
    google::InitGoogleLogging("");
    string logpath = LOGFOLDER + "recordServer-";
    google::SetLogDestination(google::GLOG_INFO,logpath.c_str());
    FLAGS_logbufsecs = 0; //缓冲日志输出，默认为30秒，此处改为立即输出
    FLAGS_max_log_size = 500; //最大日志大小为 100MB
    FLAGS_stop_logging_if_full_disk = true; //当磁盘被写满时，停止日志输出


    //获取Http API 
    url = IPPORT;
    url = url.append("main?ClientID=3312"); 
    main_ret = m_httpclient->HttpGetData(url.c_str());
    if(0 == main_ret)
    {
	   std::string resData = m_httpclient->GetResdata();
	   main_ret = parseResdata(resData, main_ret ,PARSE_TYPE::GETAPI);
	   if(0 != main_ret)
	   {
		   LOG(ERROR) << "解析Http API接口 数据失败  main_ret:"<<main_ret; 
		   return main_ret;
	   }
    }else
    {
		LOG(ERROR) << "调用Http API接口失败   main_ret:"<<main_ret;  
        return main_ret;
    }
 
   //注册录制服务接口  
   url = IpPort;
   url.append(ServerCreate);
   url.append("?serverName=");
   char *format = m_httpclient->UrlEncode(serverName);
   url.append(format);
   url.append("&serverType=1&serverApi=192.168.1.206:");
   url.append(s_http_port);    
   url.append(APIStr);
   
   main_ret = m_httpclient->HttpGetData(url.c_str());
   if(0 == main_ret)
   {
	  std::string resData = m_httpclient->GetResdata();
	  main_ret = parseResdata(resData, main_ret ,PARSE_TYPE::REGISTONLINE);
	  if(0 != main_ret)
	  {
		   LOG(ERROR) << "解析注册录制 数据失败 main_ret:"<<main_ret; 
		   //return main_ret;
	  }	  
   }else
   {  
      LOG(ERROR) << "调用注册录制服务 失败  main_ret:"<<main_ret;  
      //return main_ret;
   }

    //往消息队列里面写数据,发给监控进程
    // int msqid = getMsgQueue();

    // json obj;
    // string deleteAPI = IpPort;
    // deleteAPI.append(ServerDelete);
    // obj["serverID"] = record_serverId;
    // obj["updateAPI"] = deleteAPI;
    // std::string sendmsg = obj.dump();
    // sendMsg(msqid, CLIENT_TYPE, sendmsg.c_str());

    //信号量初始化
    main_ret = sem_init(&bin_sem, 0, 0);
    if(0 != main_ret)
    {  
      LOG(ERROR) << "bin_sem创建失败"<<" "<<"main_ret:"<<main_ret;
      return main_ret ;
    }
 
    main_ret = sem_init(&bin_blank, 0, 1000);
    if(0 != main_ret)
    {  
       LOG(ERROR) << "bin_blank创建失败"<<" "<<"main_ret:"<<main_ret;
       return main_ret ;
    }
     
    //创建录制任务管理线程
    main_ret = pthread_create(&recordManage_t, NULL, recordManage_fun, NULL);
    if(0 != main_ret)
    { 
       LOG(ERROR) << "录制管理线程创建失败"<<" "<<"main_ret:"<<main_ret; 
       return main_ret; 
    }

    //创建http服务线程  
    main_ret = pthread_create(&httpServer_t, NULL, httpServer_fun, NULL);
    if(0 != main_ret)
    { 
       LOG(ERROR) << "http服务监听线程创建失败"<<" "<<"main_ret:"<<main_ret; 
       return main_ret;   
    }

    //创建定时上传服务在线 线程
    main_ret = pthread_create(&httpTime_t,NULL, httpTime_fun, NULL);
    if(0 != main_ret)
    {
       LOG(ERROR) << "http定时上传线程创建失败"<<" "<<"main_ret:"<<main_ret; 
       return main_ret;   
    }

    //创建定时检测磁盘线程
    main_ret = pthread_create(&checkDisk_t,NULL, checkDisk_fun, NULL);
    if(0 != main_ret)
    { 
       LOG(ERROR) << "定时检测磁盘线程创建失败"<<" "<<"main_ret:"<<main_ret; 
       return main_ret;   
    }
	
    //创建定时删除录制任务线程
	main_ret = pthread_create(&deletRecordTask_t,NULL, deletRecord_fun, NULL);
	if(0 != main_ret)
    { 
       LOG(ERROR) << "定时检测磁盘线程创建失败"<<" "<<"main_ret:"<<main_ret; 
       return main_ret;   
    }
	
    return  main_ret;
}

//停止服务
int stopServer(void)
{   

    int main_ret = 0;

    //注册录制服务离线接口
   /*  main_ret = m_httpclient->HttpGetData("http://192.168.1.205:8080/live/server_create?serverType=4&serverName=serverqw&netFlag=0&serverIp=192.168.1.206:8000");
    if(main_ret != 0)
    {

       LOG(ERROR) << "注册录制服务离线失败  错误代号:"<<main_ret;  
       return main_ret;

    }else
    {
      json m_object = json::parse(m_httpclient->GetResdata());
      if(m_object.is_object())
      {
         string resCode = m_object.value("code", "oops");
         main_ret = atoi(resCode.c_str() );

         if(0 != main_ret)
         {
             std::cout<<main_ret<<endl;
             LOG(ERROR) << "注册录制服务离线失败 main_ret:"<< main_ret <<"   错误信息:"<<m_object.at("msg");
             return main_ret ;
         }
      }
    } */
  
    //定时上传录制状态线程退出
    //record_flag = 0; 
    main_ret = pthread_join(httpTime_t,NULL);
    if(0 != main_ret)
    {
       LOG(ERROR) << "定时上传录制状态线程退出错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;
    }

    //定时检测磁盘线程退出
    main_ret = pthread_join(checkDisk_t,NULL);
    if(0 != main_ret)
    {
       LOG(ERROR) << "定时检测磁盘线程退出错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;
    }
	
	//定时删除录制任务线程退出
    main_ret = pthread_join(deletRecordTask_t,NULL);
    if(0 != main_ret)
    {
       LOG(ERROR) << "定时删除录制任务线程退出错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;
    }

    //等待http服务线程退出
    //httpSev_flag = 0; 
    main_ret = pthread_join(httpServer_t, NULL);
    if(0 != main_ret)
    {
     
       LOG(ERROR) << "http服务线程退出错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;  
    } 
    printf("%s %d\n", "http服务线程退出",main_ret);
    LOG(INFO) << "http服务线程退出: "<<main_ret;


    //等待录制管理线程退出
    //recordMange_flag = 0; 
    main_ret = pthread_join(recordManage_t,NULL);
    if(0 != main_ret)
    {
       LOG(ERROR) << "录制管理线程退出错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;
    }

    printf("%s %d\n", "录制管理线程退出",main_ret);
    LOG(INFO) << "录制管理线程退出: "<<main_ret;

    /*销毁互斥*/
    main_ret = sem_destroy(&bin_sem);
    if(0 != main_ret)
    {
       LOG(ERROR) << "销毁互斥bin_sem错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;
    }		

    main_ret = sem_destroy(&bin_blank);
    if(0 != main_ret)
    {
       LOG(ERROR) << "销毁互斥bin_blank错误"<<" "<<"main_ret:"<<main_ret;
       return main_ret;
    }

    if(NULL != m_httpclient)
    {
      delete m_httpclient;
      m_httpclient = NULL;
    }
 
    return main_ret;
}

int main(int argc, char* argv[]) 
{
   int main_ret = 0;

   printf("record_Server :[tid: %ld]\n", syscall(SYS_gettid));

   //启动录制服务
   main_ret = startServer();

   if(0 != main_ret)
   {
     LOG(INFO) << "server start error:  "<<"main_ret:"<<main_ret;
     return main_ret;
   }

   printf("server starting\n");
   LOG(INFO) << "server start: "<<"main_ret:"<<main_ret;

   //停止录制服务
   main_ret = stopServer();
   if(0 == main_ret)
   {
        LOG(INFO) << "server stop 正常退出"<<" "<<"main_ret:"<<main_ret;
   }else
   {
       LOG(ERROR) << "server stop 异常退出"<<" "<<"main_ret:"<<main_ret;
       printf("server stop ret:%d\n", main_ret);
   }

   return main_ret;
}
