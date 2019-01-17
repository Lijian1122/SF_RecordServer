/*****************************************************
版权所有:北京三海教育科技有限公司
作者：lijian
版本：V0.0.1
时间：2019-01-07
功能：生成一个环形缓冲区的类，作为音视频数据的缓冲

2019.01.15 把条件变量和互斥锁封装进缓冲区类中，方便rtmp读写线程调用
****************************************************/
 
#ifndef CCycleBuffer_H
#define CCycleBuffer_H

#include <pthread.h>
#include <sys/select.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#define BUFFER_SIZE 1024*1024*10  //环形缓冲区内存大小

class CCycleBuffer
{ 
  public:
      
      CCycleBuffer(int size = BUFFER_SIZE);
	 
      ~CCycleBuffer();
	 
      /*缓冲区有效数据长度*/
      int getRetainLength();
	 
      /*往缓冲区写数据*/
      int write(char* buf,int count);
	 
      /*从缓冲区读数据*/
      int read(char* buf,int count);
	 
  private:
  
     /*写数据时,m_nWritePos以后的剩余空间足够*/
     int WriteLeftData(int m_nWritePos,char* buf,int count);

     /*读数据时, m_nReadPos以后的数据足够*/
     int ReadLeftData(int m_nReadPos,char* buf,int count);
	
 private:
 
    pthread_mutex_t mutex;    /*用于对缓冲区的互斥*/
    pthread_cond_t notempty;  /* 缓冲区非空的条件变量 */
    pthread_cond_t notfull;   /* 缓冲区未满的条件变量 */
 
    char* m_pBuf;    //缓冲区
    int m_nBufSize;  //缓冲区大小
    int m_nReadPos;  //读指针位置
    int m_nWritePos; //写指针位置
   
    bool m_bEmpty;
    bool m_bFull;
};

#endif// CCycleBuffer_H
