#include "message_queue.h"

void server()
{
    int msqid = createMsgQueue();
    char buf[MSGSIZE];
    while(1)
    {
        // 服务端先接收
        recvMsg(msqid, CLIENT_TYPE, buf); printf("客户端说：%s\n ", buf);
        printf("Please enter :");
        fflush(stdout);
        ssize_t _s = read(0, buf, sizeof(buf)-1);
        if(_s > 0)
        {
            buf[_s-1] = '\0';
            sendMsg(msqid, SERVER_TYPE, buf);

            if(strcmp(buf, "exit") == 0)
                break;
        }
    }
    destroyMsgQueue(msqid);
}


int main()
{
    server();
    return 0;
}
