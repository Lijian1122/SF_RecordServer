#include "MessageQueue.h"

int commMsg(int msgflag)
{
    // 生成IPC 关键字
    char path[1024] ;
    //获取当前的工作目录,生成关键字
    char *p = getcwd(path , 1024);

    key_t _k = ftok(p, PROJ_ID);
    int msqid = msgget(_k, msgflag); //获取消息队列ID
    if(msqid < 0)
    {
        perror("msgget");
        return -2;
    }
    return msqid;
}

//创建消息队列
int createMsgQueue() 
{
    return commMsg(IPC_CREAT|IPC_EXCL|0666);
}

//销毁消息队列
int destroyMsgQueue( int msqid) 
{
    int _ret = msgctl(msqid, IPC_RMID, 0);
    if(_ret < 0)
    {
        perror("msgctl");
        return -1;
    }
    return 0;
}

//获取消息队列
int getMsgQueue()
{
    return commMsg(IPC_CREAT);
}

//发送消息
int sendMsg( int msqid, long type,  const char *_sendInfo)
{
    msgstruct msg;
    msg.mtype = type;
    strcpy(msg.mtext, _sendInfo);

    int _snd = msgsnd(msqid, &msg, sizeof(msg.mtext), 0);
    if( _snd < 0)
    {
        perror("msgsnd");
        return -1;
    }
    return 0;
}

//接收消息
int recvMsg(int msqid, long type, char buf[])
{
    msgstruct msg;
    int _rcv = msgrcv(msqid, &msg, sizeof(msg.mtext), type, 0);
    if( _rcv < 0)
    {
        perror("msgrcv");
        return -1;
    }
    strcpy(buf, msg.mtext);
    return 0;
}
