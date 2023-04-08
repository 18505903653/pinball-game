#include <curses.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string>
#include <sys/time.h>
#include "constant.h"

//int sockfd;//创建出的连接服务端的socket_fd
struct sockaddr_in addr;//服务端的地址
int gameNo, playerNo;//当前玩家的游戏号和玩家号
int sockfdRecvFromServer;//文件描述符

int writeString(int fd, std::string s) {
    char buf[BUFFER_SIZE];
    for (int i = 0;i < s.length();i++) {
        buf[i] = s[i];
        buf[i + 1];
    }
    buf[s.length()] = '\n';
    buf[s.length() + 1] = '\0';
    return write(fd, buf, strlen(buf));
}

void showWall() {
    for (int i = 0;i < MAXX;i++) {
        move(i, 0);
        addch(WALL_CHAR);
        move(i, MAXY - 1);
        addch(WALL_CHAR);
    }
}
int refreshPicture(int x, int y, int y0, int y1) {
    //清空两个墙中间的东西
    for (int i = 0;i < MAXX;i++) {
        for (int j = 1;j < MAXY - 1;j++) {
            move(i, j);
            addch(' ');
        }
    }
    showWall();
    //显示小球
    move(x, y);
    addch(BALL_CHAR);
    //显示上下两块木板
    for (int i = y0;i < y0 + BOARD_LENGTH;i++) {
        move(0, i);
        addch(BOARD_CHAR);
    }
    for (int i = y1;i < y1 + BOARD_LENGTH;i++) {
        move(MAXX - 1, i);
        addch(BOARD_CHAR);
    }
    static timeval lastTime;
    timeval nowTime;
    gettimeofday(&nowTime, nullptr);
    int pingMs = nowTime.tv_sec * 1000 + nowTime.tv_usec / 1000 - (lastTime.tv_sec * 1000 + lastTime.tv_usec / 1000);
    lastTime = nowTime;
    move(MAXX, 1);
    printw("delay:%dms", pingMs);
    refresh();
}
int sendStart() {
    int sockfd;
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
    gameNo = atoi(gameNoStr);
    playerNo = atoi(playerNoStr);
    return 0;
}
void* receiveRun(void* args)
{
    struct sockaddr_in serveraddr;		//服务器网络信息结构体
    socklen_t addrlen = sizeof(serveraddr);
    //第二步：填充服务器网络信息结构体
    //inet_addr:将点分十进制字符串ip地址转换为整型数据
    //htons:将主机字节序转化为网络字节序
    //atoi：将数字型字符串转化为整型数据
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr("192.168.11.129");
    serveraddr.sin_port = htons(UDP_PORT);

    //第三步：进行通信
    char buf[32] = "";
    while (1) {

        char buf[32] = "";
        int n;
        if ((n = recvfrom(sockfdRecvFromServer, buf, sizeof(buf), 0, (struct sockaddr*)&serveraddr, &addrlen)) == -1) {
            perror("fail to recvfrom");
            exit(1);
        }
        buf[n] = '\0';
        char xStr[4], yStr[4], y0Str[4], y1Str[4];
        sscanf(buf, "%s%s%s%s", xStr, yStr, y0Str, y1Str);
        int x = atoi(xStr);
        int y = atoi(yStr);
        int y0 = atoi(y0Str);
        int y1 = atoi(y1Str);
        //refreshPicture(x, y, y0, y1);
        //printw("from server:%s\n", buf);
        //refresh();
        refreshPicture(x, y, y0, y1);
    }

    //第四步：关闭文件描述符
    close(sockfdRecvFromServer);
}
int sendReady() {
    if ((sockfdRecvFromServer = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");exit(1);
    }
    struct sockaddr_in udpServerAddr;
    socklen_t addrlen = sizeof(udpServerAddr);
    udpServerAddr.sin_family = AF_INET;
    udpServerAddr.sin_addr.s_addr = inet_addr("192.168.11.129");
    udpServerAddr.sin_port = htons(UDP_PORT);

    std::string msg = "ready " + std::to_string(gameNo) + " " + std::to_string(playerNo) + "\n";
    const char* buf = msg.c_str();
    sendto(sockfdRecvFromServer, buf, strlen(buf), 0, (sockaddr*)(&udpServerAddr), sizeof(udpServerAddr));
    socklen_t len = sizeof(udpServerAddr);
    char reBuf[32];
    int n = recvfrom(sockfdRecvFromServer, reBuf, 32, 0, (sockaddr*)(&udpServerAddr), &addrlen);

    pthread_t receiveThread;
    pthread_create(&receiveThread, NULL, receiveRun, NULL);
}



int sendPicture() {
    int sockfd;
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
    std::string message = "picture ";
    message = message + std::to_string(gameNo);
    writeString(sockfd, message);
    //读取x y y0 y1
    char buf[32];
    int n = read(sockfd, buf, sizeof(buf));
    buf[n] = '\0';
    close(sockfd);
    char xStr[4], yStr[4], y0Str[4], y1Str[4];
    sscanf(buf, "%s%s%s%s", xStr, yStr, y0Str, y1Str);
    int x = atoi(xStr);
    int y = atoi(yStr);
    int y0 = atoi(y0Str);
    int y1 = atoi(y1Str);
    return refreshPicture(x, y, y0, y1);
}

int sendChangeDir(int dir) {
    int sockfd;
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
    std::string message(MOVE);
    message = message + " " + std::to_string(gameNo) + " " + std::to_string(playerNo) + " " + std::to_string(dir);
    writeString(sockfd, message);
    close(sockfd);
}



int run()
{
    initscr();
    noecho();
    crmode();

    move(0, 0);
    printw("hello ,this is introduction\npress 'a' to change direction to left\npress 'd' to change direction to right\npress 'r' to be ready\npress 'q' to exit\nnow press any to start");
    refresh();

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    sendStart();

    char input;
    while ((input = getchar()) != EOF)
    {
        if (input == 'a')
        {
            sendChangeDir(-1);
        }
        else if (input == 'd')
        {
            sendChangeDir(1);
        }
        else if (input == 'r')
        {
            sendReady();
        }
        else if (input == 'q')
        {
            break;
        }
    }
    endwin();
}

int main()
{
    return run();
}