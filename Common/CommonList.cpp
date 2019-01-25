#include "CommonList.h"

CommonList::CommonList(bool lockFlag)
{
	m_lockFlag = lockFlag;
	
	if(m_lockFlag)
	{
	   m_ret = sem_init(&bin_sem, 0, 0);
          if(0 != m_ret)
          {  
             LOG(ERROR) << "bin_sem创建失败"<<" "<<"main_ret:"<<m_ret;
          }
          m_ret = sem_init(&bin_blank, 0, 1000);
         if(0 != m_ret)
         {  
            LOG(ERROR) << "bin_blank创建失败"<<" "<<"main_ret:"<<m_ret;
          }	
        }	
       m_list.clear();
}

void CommonList::pushLockList(void *data)
{
     sem_wait(&bin_blank);  			
     m_list.push_back(data);			
     sem_post(&bin_sem);  
}

void *CommonList::popLockList()
{  			
    sem_wait(&bin_sem); 

    void *data = NULL;	
    if(!m_list.empty())
    {
	  data = m_list.front();
	  m_list.pop_front();
    }
    sem_post(&bin_blank); 	
    return data;
}

void CommonList::pushList(void *data)
{			
   m_list.push_back(data);			
}

void CommonList::popList(void *data)
{
   m_list.remove(data);
}

/*查找队列中元素*/
void* CommonList::findList(void *data)
{
   std::list<void*>::iterator iter;
   void *res = NULL;
	
   iter = find_if(m_list.begin(),m_list.end(), finder_t((char*)data));
   if(iter != m_list.end())
   {
       res = *iter;
   }
   return res;
}

CommonList::~CommonList()
{
	if(m_lockFlag)
	{
	   m_ret = sem_destroy(&bin_sem);
        if(0 != m_ret)
        {
            LOG(ERROR) << "销毁互斥bin_sem错误"<<" "<<"main_ret:"<<m_ret;
        }		
        m_ret = sem_destroy(&bin_blank);
        if(0 != m_ret)
        {
           LOG(ERROR) << "销毁互斥bin_blank错误"<<" "<<"main_ret:"<<m_ret;
        }	
	}
	m_list.clear();
}
