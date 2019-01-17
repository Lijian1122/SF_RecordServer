#include "message_queue.h"

void client()
{
    int msqid = getMsgQueue();
    char buf[MSGSIZE] = "kkkkkkk";
    while(1)
    {

        sendMsg(msqid, CLIENT_TYPE, buf);

        printf("Please enter :");
        fflush(stdout);
        ssize_t _s = read(0, buf, sizeof(buf)-1);
        if(_s > 0)
        {
            buf[_s -1] = '\0';
            sendMsg(msqid, CLIENT_TYPE, buf);
        }
        recvMsg(msqid, SERVER_TYPE, buf);
        if(strcmp("exit",buf) == 0)
        {
            printf("服务端退出，客户端自动退出\n");
            break;
        }
        printf("服务端说：%s\n", buf);

        sleep(1);
    }
}

int main()
{
    client();
    return 0;
}
