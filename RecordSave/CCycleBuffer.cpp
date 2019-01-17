#include "CCycleBuffer.h"
#include <assert.h>
#include <memory.h>
 
CCycleBuffer::CCycleBuffer(int size)
{
     m_nBufSize = size; 
     m_nReadPos =0; 
     m_nWritePos =0; 
     m_pBuf = new char[m_nBufSize];
    
     test =0;
	 
     m_bEmpty =true; 
     m_bFull =false;
	
     pthread_cond_init(&notempty, NULL);
     pthread_cond_init(&notfull, NULL);
     pthread_mutex_init(&mutex, NULL);
} 

/*********向缓冲区写入数据*****/ 
int CCycleBuffer::write(char* buf,int count) 
{ 
	/*resCode 返回值 
	1.成功写入 剩余空间占用率小于0.5 返回 0;
	2.成功写入 剩余空间缓冲区占用率大于0.5 返回 1;
	3.缓冲区不足, 无剩余空间 返回 2;*/

	int resCode = 0;
	int leftcount = 0;
	
	pthread_mutex_lock(&mutex); 
     
    int leftsize = m_nBufSize - getRetainLength();
	
    if(leftsize  > count || count  ==  leftsize) //缓冲区剩余空间足够
    {	
         if(m_nReadPos > m_nWritePos)
	     {
		    //读指针和写指针之间的剩余空间
		    leftcount = m_nReadPos - m_nWritePos;	
            count = WriteLeftData(m_nWritePos,buf,count);
		    m_bFull =(m_nReadPos == m_nWritePos);  
			
	     }else
	     {
		    //写指针之后的剩余空间
		    leftcount = m_nBufSize - m_nWritePos;
		
		    //剩余空间足够放数据
		    if(leftcount > count && leftcount == count) 
            { 
               count = WriteLeftData(m_nWritePos,buf,count);
			   m_bFull =(m_nReadPos == m_nWritePos); 	
            }else
		    {
			  //先把write指针之后的剩余空间填满
			  int test = m_nWritePos;
			  memcpy(m_pBuf + test, buf, leftcount); 	 		
		      m_nWritePos =(m_nReadPos >= count - leftcount)? count - leftcount : m_nReadPos;
		
		      //在从头拷贝剩下的数据
              memcpy(m_pBuf, buf + leftcount, m_nWritePos); 
			
              count = leftcount + m_nWritePos;
              m_bFull =(m_nReadPos == m_nWritePos); 			
		    }
         
		    //判断缓冲区剩余空间是否大于50%
            if(leftsize*1.0/m_nBufSize > 0.5)  
            { 
               resCode = 1;   
            }		 
        } 		
	    pthread_mutex_unlock(&mutex);
        pthread_cond_signal(&notempty);	
	    return resCode; 
	}else
	{
	    struct timespec outtime;
        struct timeval now;
        gettimeofday(&now, NULL);
        outtime.tv_sec = now.tv_sec;
        outtime.tv_nsec = now.tv_usec*1000 + 3 * 1000 * 1000;
        outtime.tv_sec += outtime.tv_nsec/(1000 * 1000 *1000);
        outtime.tv_nsec %= (1000 * 1000 *1000);
			          
        int m_ret = pthread_cond_timedwait(&notfull, &mutex ,&outtime);
				  
	    //当有信号传过来 必须释放互斥量
	    if(m_ret == 0)
        {
		   pthread_mutex_unlock(&mutex);
	    }
		
        pthread_cond_signal(&notempty);		
		resCode = 2;
	    return resCode;
	}
} 
	
/*******从缓冲区读数据*******/
int CCycleBuffer::read(char* buf,int count) 
{ 

	/*resCode 返回值 
	1. 正常读取 返回0;
	2. 数据不足,返回1;*/
	
	int resCode = 0;
	int leftcount = 0;
	
    pthread_mutex_lock(&mutex);

	int leftdata = getRetainLength();
    if(leftdata == count || leftdata > count) //缓冲区数据足够
	{
		 if(m_nReadPos < m_nWritePos)//写在读前面，读速度稍慢于写速度
	     {	
		      leftcount = ReadLeftData(m_nReadPos,buf,count);
              m_bEmpty =(m_nReadPos == m_nWritePos); 		 
	     }else
	     {
		     leftcount = m_nBufSize - m_nReadPos;
		
		    //剩余数据够读取
            if(leftcount > count && leftcount == count)
            {
		       leftcount = ReadLeftData(m_nReadPos,buf,count);
		       m_bEmpty =(m_nReadPos == m_nWritePos);  	   
		    }else
            {
			  memcpy(buf, m_pBuf + m_nReadPos, leftcount); 
              m_nReadPos =(m_nWritePos >= count - leftcount)? count - leftcount : m_nWritePos; 
              memcpy(buf + leftcount, m_pBuf, m_nReadPos);
              m_bEmpty =(m_nReadPos == m_nWritePos); 			
		    }			
	     }
         pthread_mutex_unlock(&mutex);
         pthread_cond_signal(&notfull);		
	     return resCode;	 
	}else
	{    
         if(leftdata  == 0) //缓冲区为空，等待非空信号
	     {
		     struct timespec outtime;
             struct timeval now;
	         gettimeofday(&now, NULL);
             outtime.tv_sec = now.tv_sec;
             outtime.tv_nsec = now.tv_usec*1000 + 3 * 1000 * 1000;
             outtime.tv_sec += outtime.tv_nsec/(1000 * 1000 *1000);
             outtime.tv_nsec %= (1000 * 1000 *1000);   
	         int m_ret = pthread_cond_timedwait(&notempty, &mutex ,&outtime);
			  
	         //当有信号传过来 必须释放互斥量
		     if(m_ret == 0)
             {
		        pthread_mutex_unlock(&mutex);
		     }	
		     count = 0;
		 }else
		 {          
		     count = leftdata;
		     pthread_mutex_unlock(&mutex);		 
		 }
		 resCode = 1;
         return resCode;      		
	}	
}

//写数据时,m_nWritePos以后的剩余空间足够
int CCycleBuffer::WriteLeftData(int m_nWritePos,char* buf,int count)
{ 
     memcpy(m_pBuf + m_nWritePos, buf, count); 
     m_nWritePos += count;
     return count; 
}

//读数据时, m_nReadPos以后的数据足够
int CCycleBuffer::ReadLeftData(int m_nReadPos,char* buf,int count)
{ 
     memcpy(buf, m_pBuf + m_nReadPos, count); 
     m_nReadPos += count; 
     return count; 
} 
      
//获取缓冲区已经存入数据长度
int CCycleBuffer::getRetainLength() 
{ 
   if(m_bEmpty) 
   { 
        return 0; 	   
   }else if(m_bFull) 
   { 
       return m_nBufSize;	   
   }else if(m_nReadPos < m_nWritePos) 
   { 
       return m_nWritePos - m_nReadPos;  
   }else 
   { 
      return m_nBufSize - m_nReadPos + m_nWritePos; 
   } 
} 

CCycleBuffer::~CCycleBuffer() 
{   
    pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&notempty);
    pthread_cond_destroy(&notfull);
	
	if(NULL != m_pBuf)
	{
		delete[] m_pBuf; 
		m_pBuf = NULL;
	}
}
