#include "webserver.h"

//处理Http请求
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
       if(strcmp(methodStr, APIStr.c_str()) == 0)
       {
          LOG(INFO) << "开始解析:"<<methodStr;
		 
          //解析http请求参数
		  int parmlen = (int)hm->query_string.len;
          char parmStr[parmlen];
          sprintf(parmStr, "%.*s",parmlen,hm->query_string.p);
		 
	      printf("url参数长度：%d  %s\n",parmlen ,parmStr);
  
          if(parmlen == 0 ||  parmlen > 1024)
          {
              LOG(ERROR)<<"Url Error 方法后 参数为空或参数太长";
              ret = RESCODE::URL_ERROR;
              goto end;
          }
          char  liveId_buf[1024] = {0};
          char  type_buf[1024] = {0};
		 
          mg_get_http_var(&hm->query_string, "liveId", liveId_buf, parmlen); //获取liveID
          mg_get_http_var(&hm->query_string, "type", type_buf, parmlen);  //获取录制命令
		    
          LOG(INFO) << "获取参数 : "<<liveId_buf<<"  "<<type_buf;
          if((strcmp(liveId_buf,"")  == 0 ) ||  (strcmp(type_buf,"") == 0))
          {
              LOG(ERROR)<<"参数为空 直播ID或操作类型为空";
              ret = RESCODE::LIVEID_ERROR;
              goto end;
          }

          int cmdType = atoi(type_buf);
          liveParmStruct *m_parmData = new liveParmStruct;
          if(NULL != m_parmData)
          {
               m_parmData->liveID = liveId_buf;
               m_parmData->cdmType = (RECORDCMD)cmdType;

               //参数入 直播参数队列
               LiveParmList->pushLockList((void*)m_parmData);
          }else
          {
              LOG(ERROR)<<"malloc 失败";
              ret = RESCODE::MALLOC_ERROR;
              goto end;
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

//处理录制参数线程
void *recordManage_fun(void *data)
{
    int ret = 0;
    void *parmdata = NULL;
    while(recordMange_flag)
    {      
       //录制参数出 参数队列
       parmdata = LiveParmList->popLockList();
       if(NULL != parmdata)
       {
	     liveParmStruct *pdata = (liveParmStruct*)parmdata;
         LOG(INFO) << "管理线程获取参数 直播ID:"<<pdata->liveID<<"  Type:"<<pdata->cdmType;

         if(pdata->cdmType == RECORDCMD::START) //开始录制
         {      
			 //判断liveID是否已经存在
			 if(NULL == RecordSaveList->findList((void*)pdata->liveID.c_str()))
			 {
				  //新建录制对象,并启动录制
				  RecordSaveRunnable *recordRun = new RecordSaveRunnable(pdata->liveID.c_str());
				  if(NULL != recordRun)
                  {
                      ret = recordRun->StartRecord();
                      if(ret == 0)//启动成功
                      {
                          //录制对象入队列
                          RecordSaveList->pushList((void*)recordRun);
                          LOG(INFO) << "启动录制任务成功   ret:"<<ret<<"   直播ID:"<<pdata->liveID;
                      }else
                      {
                          LOG(ERROR) << "启动录制任务失败  ret:"<<ret<<"   直播ID:"<<pdata->liveID;
                          if(NULL != recordRun)
                          {
                              delete recordRun;
                              recordRun = NULL;
                          }
                      }
                  }else {
                         LOG(INFO) << "创建录制对象失败  直播ID:"<<pdata->liveID;
                  }
			 }else
			 {
                  LOG(ERROR) << "该录制任务正在录制中  直播ID:"<<pdata->liveID;         
             }			
        }else if(pdata->cdmType == RECORDCMD::STOP)  //停止录制
        {
			 //判断liveID是否已经存在
			 void *recordData = RecordSaveList->findList((void*)pdata->liveID.c_str());
			 if(NULL != recordData)//在录制对象队列中找到找到liveID
			 {	
                 //停止录制任务		 
			     RecordSaveRunnable *m_runnable = (RecordSaveRunnable*)recordData;
				 m_runnable->SetStopFlag();
				 
                 //录制对象出队列	 
				 RecordSaveList->popList(recordData);
				 		 
				 //删除任务入队列
                 DeleteRecordList->pushLockList(recordData);				 
			 }else
             {
				 LOG(ERROR) << "未找到该录制任务  直播ID:"<<pdata->liveID;            
			 }				 
        }else{
          LOG(ERROR) << "未知的命令 type:"<<pdata->liveID;
        }
        //删除结构体
        delete pdata;
         pdata = NULL;
       }
   }
   return data;
}

//停止录制任务 线程
void *stopRecord_fun(void *data)
{	
    int ret = 0;
    void *m_data = NULL;
    while(recordMange_flag)
    {   
        //删除任务出 删除队列
        m_data = DeleteRecordList->popLockList();
      
        if(NULL != m_data)
        {
            RecordSaveRunnable *m_runnable = (RecordSaveRunnable*)m_data;
            string liveId = m_runnable->GetRecordID();

            ret = m_runnable->StopRecord(); 	 
	        if(0 == ret)
            {
                 LOG(INFO)<< "停止录制任务成功 ret:"<<ret<<"   直播ID:"<<m_runnable->GetRecordID();                
            }else
            {
                 LOG(ERROR) << "停止录制任务失败 ret:"<<ret<<"   直播ID:"<<m_runnable->GetRecordID();
            }
	
	        //删除录制对象
	        if(NULL != m_runnable)
            {
		        delete m_runnable;
	            m_runnable = NULL;
                LOG(INFO) << "已经删除录制任务 ret:"<<ret<<"   直播ID:"<<liveId;
	        }
        }
    }
	return data;
}

//解析Http返回json数据
int parseResdata(string &resdata,PARSE_TYPE m_Type)
{     
    int ret = 0;
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
			            json data_object = m_object.at("data");
						
		                string IP = data_object.value("ip", "oops");
                        string port = data_object.value("port", "oops");
                        IpPort = IpPort.append(IP).append(":").append(port);

                        ServerCreate = data_object.value("server_create", "oops");
                        ServerDelete = data_object.value("server_delete", "oops");
                        ServerSelect = data_object.value("server_select", "oops");
                        ServerUpdate = data_object.value("server_update", "oops");

                        liveUpdate = data_object.value("live_update", "oops");
                        liveSelect = data_object.value("live_select", "oops");
                        liveUpload = data_object.value("live_upload", "oops");

                        cout<<IpPort  <<ServerCreate  <<ServerDelete  <<ServerSelect  <<liveUpdate <<endl;
                        LOG(INFO)<<"获取API参数如下："<<IpPort<<"  "<<ServerCreate<<"  "<<ServerDelete<<"  "<<ServerSelect
                                 <<"  "<<liveUpdate;
                        break;
	               }
			       case PARSE_TYPE::REGISTONLINE:  //解析注册录制服务
	               {
				       record_serverId = m_object.value("serverId", "oops");
                       LOG(INFO)<<"录制服务 record_serverId:"<<record_serverId;	
                       printf("serverID: %s\n", record_serverId.c_str());	
			           break;
	               }
			       case PARSE_TYPE::UPDATA:  //解析定时上传录制在线
	               {
                       LOG(INFO)<<"定时上传录制服务在线状态  record_serverId:"<<record_serverId<<"  ret:"<<ret;
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

//上传录制在线
void updateOnline_fun()
{
    int main_ret = s_httpclient->HttpGetData(updateOnlineUrl.c_str());
    if(0 == main_ret)
    {
	    std::string resData = s_httpclient->GetResdata();
        main_ret = parseResdata(resData, PARSE_TYPE::UPDATA);
	    if(0 != main_ret)
	    {
	       LOG(ERROR) << "解析定时返回数据失败  main_ret:"<<main_ret;
	    }
    }else
    {
       LOG(ERROR) << "调用定时上在线状态接口失败  main_ret:"<<main_ret;
    }	
}

//检测磁盘空间
void checkdisk_fun()
{
    char path[1024] ;
    char *p = getcwd(path , 1024); //获取当前路径
    printf("current dir:%s\n", p);

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
       LOG(ERROR)<<"磁盘空间即将不足 剩余空间:"<<mbFreedisk;
    }
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
		case TIMER_TYPE::CHEDISK:  //定时检测磁盘空间
	    {
			 checkdisk_fun();
			 break;
	    }
	 }
  }while(err<0 && errno==EINTR);
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
   updateOnlineUrl.append("?serverId=");
   
   updateOnlineUrl.append(record_serverId);
   updateOnlineUrl.append("&netFlag=1");
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
   
   printf("启动Http服务 port:%s\n", ServerPort.c_str());
   LOG(INFO) << "Http服务启动  port:"<<ServerPort.c_str();
   nc = mg_bind(&mgr, ServerPort.c_str(), ev_handler);
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

    ServerPort  = config_file.GetConfigName("ServerPort");
    FILEFOLDER = config_file.GetConfigName("Filefolder");
    IpPort = config_file.GetConfigName("IpPort");
    APIStr = config_file.GetConfigName("APIStr");
    HttpAPIStr = config_file.GetConfigName("HttpAPIStr");
    LOGFOLDER  = config_file.GetConfigName("Logfolder");
    ServerName = config_file.GetConfigName("ServerName");
    ServerNameAPIStr = config_file.GetConfigName("ServerNameAPI");
    ServerCreateStr = config_file.GetConfigName("ServerCreateAPI");

    string aacTagCountStr = config_file.GetConfigName("AacTagCount");
    aacTagCount = atoi(aacTagCountStr);

    if(!ServerPort || !FILEFOLDER || !IpPort || !APIStr || !HttpAPIStr || !LOGFOLDER ||
        !ServerName || !ServerNameAPIStr || !ServerCreateStr || !aacTagCountStr)
    {
        LOG(ERROR) << "获取配置文件字段为空  "<<"   main_ret:"<<main_ret;
        return main_ret ;
    }
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
    main_ret = m_httpclient->HttpGetData(HttpAPIStr.c_str());
    if(0 == main_ret)
    {
	   std::string resData = m_httpclient->GetResdata();
	   main_ret = parseResdata(resData,PARSE_TYPE::GETAPI);
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
   url.append(ServerNameAPIStr);
   char *format = m_httpclient->UrlEncode(ServerName);
   url.append(format);
   url.append(ServerCreateStr);
   url.append(ServerPort);    
   url.append(APIStr);
   
   main_ret = m_httpclient->HttpGetData(url.c_str());
   if(0 == main_ret)
   {
	  std::string resData = m_httpclient->GetResdata();
	  main_ret = parseResdata(resData ,PARSE_TYPE::REGISTONLINE);
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

    //初始化三个队列
    LiveParmList = new CommonList(true);
    if(0 != LiveParmList->getRescode())
    {
       LOG(ERROR) << "初始化参数队列失败  main_ret:"<<LiveParmList->getRescode();
       return main_ret;
    }
    RecordSaveList = new CommonList(false);
  
    DeleteRecordList = new CommonList(true);
    if(0 !=  DeleteRecordList->getRescode())
    {
       LOG(ERROR) << "初始化删除对象队列失败 main_ret:"<< DeleteRecordList->getRescode();
       return main_ret;
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
	
    //创建删除录制任务线程
	main_ret = pthread_create(&deletRecordTask_t,NULL, stopRecord_fun, NULL);
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
    
    //销毁所有队列对象
    if(NULL != LiveParmList)
    {
	delete LiveParmList;
	LiveParmList = NULL;
    }
    if(NULL != RecordSaveList)
    {
	delete RecordSaveList;
	RecordSaveList = NULL;
    }
    if(NULL != DeleteRecordList)
    {
	delete DeleteRecordList;
	DeleteRecordList = NULL;
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
