#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>

#include "Base/base.h"

#include "glog/logging.h"

#define LOG_FILE "./log/recordmonitor."

int main(int argc, char **argv) 
{
    int ret, i, status;
    char *child_argv[100] = {0};
    pid_t pid;
    char buff[1024];

    //创建log初始化
    google::InitGoogleLogging("");
    google::SetLogDestination(google::GLOG_INFO,LOG_FILE);
    FLAGS_logbufsecs = 0; //缓冲日志输出，默认为30秒，此处改为立即输出
    FLAGS_max_log_size = 500; //最大日志大小为 100MB
    FLAGS_stop_logging_if_full_disk = true; //当磁盘被写满时，停止日志输出

    if (argc < 2) 
    {	
        sprintf(buff,"Usage:%s <exe_path> <args...>", argv[0]);     
        LOG(ERROR) << buff;
		return -1;
    }
	
    for(i = 1; i < argc; ++i) 
    {
        child_argv[i-1] = (char *)malloc(strlen(argv[i])+1);
        strncpy(child_argv[i-1], argv[i], strlen(argv[i]));   
    }
    while(1)
    {
        pid = fork(); 

        printf("pid: %d\n", pid);

        LOG(INFO) << "pid :"<< (int)pid;
        if(pid == -1) 
        {
            printf("fork() error.errno:%d error:%s", errno, strerror(errno));
            LOG(ERROR)<<"fork() error.errno error:"<< strerror(errno);
            break;

        }else if(pid == 0) 
        {
		
            LOG(ERROR)<<"serverName:"<<child_argv[0];
            ret = execv(child_argv[0], (char **)child_argv); 
           		
            if (ret < 0) 
            {
                printf("Child process execv ret:%d errno:%d error:%s",ret, errno, strerror(errno));
                sprintf(buff,"Child process ret:%d errno:%d error:%s",ret, errno, strerror(errno));                
                LOG(ERROR)<<buff;
                break;
            }
            
        }else
        {
            waitpid(pid, &status,0);
            printf("Child process id: %d \n", pid);
            LOG(INFO)<<"hild process id: "<<(int)pid;

            //检测到录制服务挂了，进入循环再次启动录制服务
            printf("Child process exit with status: %d\n",status);
            LOG(INFO)<<"Child process exit with status:"<<status;

            //确保进程已经杀死
            kill(pid, SIGTERM);
        }
    }

    printf("Parent process exit!\n");
    LOG(INFO)<<"Parent process exit!";

    return ret;
}
