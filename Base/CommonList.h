/*****************************************************
版权所有:北京三海教育科技有限公司
作者：lijian
版本：V0.0.1
时间：2018-01-23
功能：把线程同步操作队列的过程封装为一个类，方便调用
1.数据结构为链表；
2.线程同步用的是信号量；
3.实现了队列的基本操作，包括入队,出队,查询等的操作
******************************************************/
#ifndef COMMONLIST_H
#define COMMONLIST_H

#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <list>
#include <iostream>
#include <algorithm>
//#include "../RecordSave/RecordSaveRunnable.h"
#include "glog/logging.h"


/*根据liveId查找队列中的录制对象*/
typedef struct finder_t
{
  finder_t(char *n)
    : rID(n)
  {
  }
  bool operator()(void *data)
  {  
     // RecordSaveRunnable *m_runnable = (RecordSaveRunnable*)data;
    //  string Id = m_runnable->GetRecordID();
    //  return (strcmp(rID ,Id.c_str()) == 0);
     return false;
  }
  char *rID;
}finder_t;


/*线程同步信号量类封装*/
class CommonList
{
public:
    CommonList(bool lockFlag);
    ~CommonList();
    int getRescode(){return m_ret;};
    
	/*加锁入队列*/
    void pushLockList(void* data);
	
	/*加锁出队列*/
    void* popLockList();
	
	/*不加锁入队列*/
	void pushList(void* data);
	
	/*不加锁出队列*/
	void popList(void *data);
	
	/*查找队列中元素*/
	void* findList(void*);
	
private:
   
   /*信号量*/
   sem_t bin_sem;
   sem_t bin_blank;	
   
   int m_ret;
   
   /*数据队列*/
   std::list<void*> m_list;
   
   /*是否需要加锁*/
   bool m_lockFlag;
};

#endif // COMMONLIST_H
