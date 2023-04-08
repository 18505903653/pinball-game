//UDP客户端的实现
#include <stdio.h>			//printf
#include <stdlib.h>			//exit
#include <sys/types.h>
#include <sys/socket.h>		//socket
#include <netinet/in.h>		//sockaddr_in
#include <arpa/inet.h>		//htons	inet_addr
#include <unistd.h>
#include <string.h>

int sendStart(int udpPort) {
    int sockfd;
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1);
    }
    if (connect(sockfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("connect");
        return 1;
    }
    char buf[128];
    //发送start请求
    strcpy(buf, "start\n");
    write(sockfd, buf, strlen(buf));
    //接收游戏号和玩家号
    int n = read(sockfd, buf, sizeof(buf));
    buf[n] = '\0';
    close(sockfd);
    char gameNoStr[10], playerNoStr[4];
    sscanf(buf, "%s%s", gameNoStr, playerNoStr);
    int gameNo = atoi(gameNoStr);
    int playerNo = atoi(playerNoStr);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 1) {
        //fprintf(stderr, "Usage: %s <ip> <port>\n", agrv[0]);
        exit(1);
    }

    int sockfd;		//文件描述符
    struct sockaddr_in serveraddr;		//服务器网络信息结构体
    socklen_t addrlen = sizeof(serveraddr);

    //第一步：创建套接字
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("fail to socket");
        exit(1);
    }

    //第二步：填充服务器网络信息结构体
    //inet_addr:将点分十进制字符串ip地址转换为整型数据
    //htons:将主机字节序转化为网络字节序
    //atoi：将数字型字符串转化为整型数据
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serveraddr.sin_port = htons(8081);


    //第三步：进行通信
    char buf[32] = "";
    sendStart(sockfd);
    while (1) {
        char text[32] = "";
        printf("begin recvfrom\n");
        if (recvfrom(sockfd, text, sizeof(text), 0, (struct sockaddr*)&serveraddr, &addrlen) == -1) {
            perror("fail to recvfrom");
            exit(1);
        }
        printf("from server:%s\n", text);
    }

    //第四步：关闭文件描述符
    close(sockfd);

    return 0;
}
