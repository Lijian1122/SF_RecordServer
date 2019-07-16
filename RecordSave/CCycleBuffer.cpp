#include "CCycleBuffer.h"
#include <assert.h>
#include <memory.h>
#include <stdio.h>
 
CCycleBuffer::CCycleBuffer(int size)
{
     m_nBufSize = size; 
     m_nReadPos =0; 
     m_nWritePos =0; 
     m_pBuf = new char[m_nBufSize];
     memset(m_pBuf,0,m_nBufSize);
    
     m_usedSize = 0;
	
     pthread_cond_init(&notempty, NULL);
     pthread_cond_init(&notfull, NULL);
     pthread_mutex_init(&mutex, NULL);
} 

/*********向缓冲区写入数据*****/ 
int CCycleBuffer::write(char* buf,int count) 
{ 
	/*resCode 返回值 
	1.成功写入 已用空间占用率小于0.5 返回 0;
	2.成功写入 已用空间占用率大于0.5 返回 1;
	3.缓冲区不足, 无剩余空间 返回 2;*/

	pthread_mutex_lock(&mutex); 
	int resCode = 0;
	int leftcount = 0;
   
    //缓冲区剩余空间
    int leftSize = m_nBufSize - m_usedSize;
	
    if(leftSize > count || count == leftSize) //缓冲区剩余空间足够
    {	
         if(m_nReadPos > m_nWritePos) //读的位置在写位置前面
	     {
		    //剩余空间为读指针和写指针之间的空间
		     memcpy(m_pBuf + m_nWritePos, buf, count); 
             m_nWritePos += count;
	     }else
	     {
		    //写指针之后的剩余空间
		    leftcount = m_nBufSize - m_nWritePos;	    
		    if(leftcount > count  || leftcount == count)//一次可以写入
            { 
                memcpy(m_pBuf + m_nWritePos, buf, count); 
                m_nWritePos += count;
				
            }else //两次写入
		    {
			  //先把write指针之后的剩余空间填满
			  memcpy(m_pBuf + m_nWritePos, buf, leftcount); 	 		
		      m_nWritePos = count - leftcount;
		      
			  //在从头拷贝剩下的数据
              memcpy(m_pBuf, buf + leftcount, m_nWritePos); 		    			
		   }		   
        } 
		
        m_usedSize += count; 
		
		//判断空间使用率是否大于50%
        if(m_usedSize *1.0/m_nBufSize > 0.5)  
        { 
             resCode = 1;   
        }	
	}else //缓冲区剩余空间不足
	{
	    struct timespec outtime;
	    struct timeval now;
	    gettimeofday(&now, NULL);
	    outtime.tv_sec = now.tv_sec;
	    outtime.tv_nsec = now.tv_usec*1000 + 3 * 1000 * 1000;
	    outtime.tv_sec += outtime.tv_nsec/(1000 * 1000 *1000);
	    outtime.tv_nsec %= (1000 * 1000 *1000);
			          
	    pthread_cond_timedwait(&notfull, &mutex ,&outtime);
	    resCode = 2;
	}

    pthread_mutex_unlock(&mutex);    
    pthread_cond_signal(&notempty);	
	return resCode;
} 
	
/*******从缓冲区读数据*******/
int CCycleBuffer::read(char* buf,int count, bool resetFlag)
{ 

	/*resCode 返回值 
	1. 正常读取 返回0;
	2. 缓冲区不为空，但数据不足,返回1;
	3. 缓冲区为空，返回2*/
	
    pthread_mutex_lock(&mutex);
	int leftcount = 0;
	int resCode = 0;
    if(m_usedSize == count || m_usedSize > count) //缓冲区数据足够
	{
		 if(m_nReadPos < m_nWritePos)//写在读前面，读速度稍慢于写速度
	     {     
		     memcpy(buf, m_pBuf + m_nReadPos, count); 
             m_nReadPos += count; 		 
	     }else
	     {
		     leftcount = m_nBufSize - m_nReadPos; 
             if(leftcount > count || leftcount == count) //剩余数据够读取,一次写入
             {
		        memcpy(buf, m_pBuf + m_nReadPos, count); 
                m_nReadPos += count; 
				
		     }else  //两次写入
             { 
			    memcpy(buf, m_pBuf + m_nReadPos, leftcount); 
                m_nReadPos = count - leftcount; 
                memcpy(buf + leftcount, m_pBuf, m_nReadPos);		
		     }			
	     }	 
         m_usedSize -= 	count;	
	}else
	{
         if(m_usedSize == 0) //缓冲区为空，等待非空信号
	     {
	         resCode = 2;
	     }else
	     {
			 resCode = 1;
	     }

		 LOG(ERROR) <<"缓冲区数据不足 m_usedSize:"<<m_usedSize<<"  count:"<<count;
		 
	     //rtmp异常读不到数据,重初始化缓冲区
	     if(resetFlag)
         {
			 LOG(ERROR) <<"rtmp读数据超时，读不到一个Tag 重置缓冲区, 等待下次重连  m_usedSize:"<<m_usedSize<<"  count:"<<count;
			 m_nBufSize = size;
			 m_nReadPos =0;
			 m_nWritePos =0;
			 memset(m_pBuf,0,m_nBufSize);
			 m_usedSize = 0;
         }
		 
         struct timespec outtime;
         struct timeval now;
         gettimeofday(&now, NULL);
         outtime.tv_sec = now.tv_sec;
         outtime.tv_nsec = now.tv_usec*1000 + 3 * 1000 * 1000;
         outtime.tv_sec += outtime.tv_nsec/(1000 * 1000 *1000);
         outtime.tv_nsec %= (1000 * 1000 *1000);
         int m_ret = pthread_cond_timedwait(&notempty, &mutex ,&outtime);
    }

	pthread_mutex_unlock(&mutex);
	pthread_cond_signal(&notfull);
	return resCode;   
}

CCycleBuffer::~CCycleBuffer() 
{   
    if(NULL != m_pBuf)
	{
		delete[] m_pBuf; 
		m_pBuf = NULL;
	}
    pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&notempty);
    pthread_cond_destroy(&notfull);	
}
