#ifndef _COMMON_H_
#define _COMMON_H_
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PATHNAME "22"
#define PROJ_ID  0x666
#define MSGSIZE 1024

#define SERVER_TYPE 1  //服务端发送消息类型
#define CLIENT_TYPE 2   //客户端发送消息类型

typedef struct msgstruct          // 消息结构
{
    long mtype;     // 消息类型
    char mtext[MSGSIZE]; // 消息buf
}msgstruct;

int createMsgQueue();  // 创建消息队列
int destroyMsgQueue(int msqid); // 销毁消息队列

int getMsgQueue();     // 获取消息队列

int sendMsg(int msqid, long type,  const char *_sendInfo);   // 发送消息
int recvMsg(int msqid, long type, char buf[]);       // 接收消息

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H*/
