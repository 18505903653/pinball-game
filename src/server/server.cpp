#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <aio.h>
#include <unordered_map>
#include <sys/time.h>
#include "constant.h"

// class Server {
// private:

typedef struct board
{
    int x, y;//木板左起点坐标
    timeval lastTime;//木板上一次移动的时间
    int turn;//木板运动方向
} Board;

typedef struct ball
{
    int x;//小球竖坐标
    int y;//小球横坐标
    int horizon;//小球水平速度方向
    int vertical;//小球吹制速度方向
    int alive;//小球存活0死1获
} Ball;

typedef struct game
{
    int playerNum;//玩家数量，等于0表示不进行
    bool ready[2];//两位玩家的准备情况
    int win;//获胜玩家 0表示上方玩家，1表示下方玩家
    sockaddr_in* addrs[2];//两个玩家的网络地址
    //char str[MAXX][MAXY];//游戏画面
    Board boards[2];//两块木板
    Ball ball;//一个球
} Game;

Game games[GAME_N];
fd_set allset;
int client[FD_SETSIZE];
int requestCnt = 0;
int udpServerSockfd;
std::unordered_map<int, sockaddr_in*> sockaddrs;

void aioWriteCompletionHandle(int sig, siginfo_t* info, void* ucontext) {
    //用来获取读aiocb结构的指针
    aiocb* prd;
    int ret;
    prd = (struct aiocb*)info->si_value.sival_ptr;
    //判断请求是否成功
    if (aio_error(prd) == 0)
    {
        //获取返回值
        ret = aio_return(prd);
        char* response = (char*)prd->aio_buf;
        //printf("aio_write ok fd = %d,buf = %s", prd->aio_fildes, response);
        close(prd->aio_fildes);
        delete[] response;
        delete prd;
        ////printf("读返回值为:%d\n", ret);
    }
}

void aioWriteChars(int fd, char* buf) {
    aiocb* cb = new aiocb();
    bzero(cb, sizeof(*cb));
    cb->aio_buf = buf;
    cb->aio_fildes = fd;
    cb->aio_offset = 0;
    cb->aio_nbytes = BUFFER_SIZE;

    //填充aiocb中有关回调通知的结构体sigevent
    cb->aio_sigevent.sigev_notify = SIGEV_SIGNAL;//使用信号回调通知
    cb->aio_sigevent.sigev_signo = O_SIGNAL;//设置通知信号
    //cb->aio_sigevent.sigev_notify_attributes = nullptr;//使用默认属性
    cb->aio_sigevent.sigev_value.sival_ptr = cb;//在aiocb控制块中加入自己的位置指针
    //printf("begin aio_write fd = %d,buf = %s", fd, buf);
    aio_write(cb);
}

void aioWriteString(int fd, std::string s) {
    char* buf = new char[BUFFER_SIZE];
    for (int i = 0;i < s.length();i++) {
        buf[i] = s[i];
        buf[i + 1];
    }
    buf[s.length()] = '\n';
    buf[s.length() + 1] = '\0';
    aioWriteChars(fd, buf);
}

//
int writeString(int fd, std::string s) {
    //std::cout << s << std::endl;
    char buf[BUFFER_SIZE];
    for (int i = 0;i < s.length();i++) {
        buf[i] = s[i];
        buf[i + 1];
    }
    buf[s.length()] = '\n';
    buf[s.length() + 1] = '\0';
    ////printf("write:%s", buf);
    return write(fd, buf, strlen(buf));
}

//初始化游戏
void initialGame(int no) {
    //游戏玩家状态
    games[no].win = -1;
    games[no].ready[0] = games[no].ready[1] = false;
    //初始化小球状态
    games[no].ball.alive = 1;
    games[no].ball.horizon = RIGHT;
    games[no].ball.vertical = DOWN;
    games[no].ball.x = MAXX / 2;
    games[no].ball.y = 1;
    //初始化0号木板状态
    games[no].boards[0].x = 0;
    games[no].boards[0].y = 1;
    games[no].boards[0].turn = LEFT;
    //初始化1号木板状态
    games[no].boards[1].x = MAXX - 1;
    games[no].boards[1].y = 1;
    games[no].boards[1].turn = LEFT;
}

//处理开始游戏请求
int handle_start(int socket)
{
    ////printf("call handle_start(%d)\n", socket);
    static int oneWaitNo = -1;
    int firstZero = -1;
    bool ok = false;
    int gameNo, playerNo;
    if (oneWaitNo >= 0) {
        games[oneWaitNo].playerNum++;
        gameNo = oneWaitNo;
        playerNo = 1;
        oneWaitNo = -1;
        ok = true;
    }
    else {
        for (int i = 0;i < GAME_N;i++) {
            if (games[i].playerNum == 0) {
                games[i].playerNum++;
                gameNo = i;
                playerNo = 0;
                ok = true;
                oneWaitNo = i;
                initialGame(gameNo);
                break;
            }
        }
    }
    if (ok) {
        games[gameNo].addrs[playerNo] = sockaddrs[socket];
        sockaddrs[socket] = nullptr;
        std::string message = std::to_string(gameNo) + " " + std::to_string(playerNo);
        aioWriteString(socket, message);
    }
    else {
        std::string message = std::to_string(-1) + " " + std::to_string(-1);
        aioWriteString(socket, message);
    }
    ////printf("%d start in the %d\n", socket, ret);
    return 0;
}

//处理udp请求
int handle_udpRequest(int socket, sockaddr_in* sockAddrClient, socklen_t* len, char* request)
{
    ////printf("call handle_ready(%d)\n", socket);
    char cmd[10], gameNoStr[10], playerNoStr[2];
    sscanf(request, "%s%s%s", cmd, gameNoStr, playerNoStr);
    if (strcmp(cmd, "ready") == 0) {
        //printf("cmd = ready\n");
        int gameNo = atoi(gameNoStr);
        int playerNo = atoi(playerNoStr);
        //int portNum = atoi(portNumStr);
        //handle_ready(socket, gameNo, playerNo);
        games[gameNo].ready[playerNo] = true;
        games[gameNo].addrs[playerNo] = sockAddrClient;
        sendto(udpServerSockfd, "OK\n", strlen("OK\n"), 0, (sockaddr*)sockAddrClient, socklen_t(sizeof(sockAddrClient)));
        //printf("send to port = %d\n", sockAddrClient->sin_port);
    }

    //games[gameNo].addrs[playerNo]->sin_port = portNum;
    //aioWriteString(socket, "OK");
    return 0;
}

//处理准备请求
int handle_ready(int socket, int gameNo, int playerNo, int portNum)
{
    ////printf("call handle_ready(%d)\n", socket);
    games[gameNo].ready[playerNo] = true;
    games[gameNo].addrs[playerNo]->sin_port = portNum;
    aioWriteString(socket, "OK");
    return 0;
}
void udpSendPicture(int gameNo);
//处理变向请求
int handle_changeDir(int socket, int gameNo, int playerNo, int dir) {
    timeval now;
    gettimeofday(&now, nullptr);
    timeval lastTime = games[gameNo].boards[playerNo].lastTime;
    bool timeOk = true;
    if (now.tv_sec * 1000000 + now.tv_usec - (lastTime.tv_sec * 1000000 + lastTime.tv_usec) < BOARD_SLEEP) {
        timeOk = false;
    }
    //可以移动
    if (timeOk && games[gameNo].boards[playerNo].y + dir >= 1 && games[gameNo].boards[playerNo].y + BOARD_LENGTH - 1 + dir <= MAXY - 2) {
        games[gameNo].boards[playerNo].y += dir;
        games[gameNo].boards[playerNo].lastTime = now;
        udpSendPicture(gameNo);
    }
    close(socket);
    return 0;
}
//处理获取画面请求
int handle_picture(int socket, int gameNo) {
    ////printf("call handle_picture(%d,%d\n", socket, gameNo);
    char buf[20];
    int x = games[gameNo].ball.x;
    int y = games[gameNo].ball.y;
    int y0 = games[gameNo].boards[0].y;
    int y1 = games[gameNo].boards[1].y;
    ////printf("x = %d y = %d y0 = %d y1 = %d\n", x, y, y0, y1);
    std::string picture = std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(y0) + " " + std::to_string(y1);
    aioWriteString(socket, picture);
    return 0;
}
//处理总请求
int handle_request(int socket, char* request)
{
    ////printf("call handle_request(%d,%s", socket, request);
    char cmd[20];
    sscanf(request, "%s", cmd);
    // if (strcmp(cmd, "picture")) {
    //     //printf("call handle_request(%d,%s)\n", socket, request);
    // }
    if (strcmp(cmd, START) == 0)
    {
        handle_start(socket);
    }
    else if (strcmp(cmd, READY) == 0)
    {
        char gameNoStr[10], playerNoStr[2], portNumStr[10];
        sscanf(request, "%s%s%s%s", cmd, gameNoStr, playerNoStr, portNumStr);
        int gameNo = atoi(gameNoStr);
        int playerNo = atoi(playerNoStr);
        int portNum = atoi(portNumStr);
        handle_ready(socket, gameNo, playerNo, portNum);
    }
    else if (strcmp(cmd, MOVE) == 0)
    {
        char dirStr[5], gameNoStr[10], playerNoStr[2];
        sscanf(request, "%s%s%s%s", cmd, gameNoStr, playerNoStr, dirStr);
        int gameNo = atoi(gameNoStr);
        int playerNo = atoi(playerNoStr);
        int dir = atoi(dirStr);
        handle_changeDir(socket, gameNo, playerNo, dir);
    }
    else if (strcmp(cmd, "picture") == 0) {
        ////printf("cmd == picture\n");
        char gameNoStr[10];
        sscanf(request, "%s%s", cmd, gameNoStr);
        int gameNo = atoi(gameNoStr);
        handle_picture(socket, gameNo);
    }
    return 0;
}

//一局游戏的小球移动函数
void ballMove(int no) {
    //char(*str)[MAXX][MAXY] = &(games[no].str);
    Ball* ball = &(games[no].ball);
    //(*str)[ball->x][ball->y] = ' ';
    int x = ball->x, y = ball->y;
    int h = ball->horizon, v = ball->vertical;
    //判断小球在水平方向上的碰撞
    if (h == RIGHT && y + 1 >= MAXY - 1) {
        ball->horizon = LEFT;
    }
    else if (h == LEFT && y - 1 <= 0) {
        ball->horizon = RIGHT;
    }
    //判断小球在竖直方向上的碰撞
    bool upCollide = games[no].boards[0].y <= y && y < games[no].boards[0].y + BOARD_LENGTH;
    bool dwCollide = games[no].boards[1].y <= y && y < games[no].boards[1].y + BOARD_LENGTH;
    if (v == UP && x - 1 <= 0) {
        ball->vertical = DOWN;
    }
    else if (v == DOWN && x + 1 >= MAXX - 1) {
        ball->vertical = UP;
    }
    //小球在水平方向上的移动
    if (ball->horizon == RIGHT)
    {
        ball->y++;
    }
    else if (ball->horizon == LEFT)
    {
        ball->y--;
    }
    //小球在竖直方向上的移动
    if (ball->vertical == DOWN)
    {
        ball->x++;
    }
    else if (ball->vertical == UP)
    {
        ball->x--;
    }
    if (ball->x >= MAXX - 1 || ball->x <= 0)
    {
        if (ball->x >= MAXX - 1)
        {
            games[no].win = 0;
        }
        else
        {
            games[no].win = 1;
        }
        ball->alive = 0;
    }
}

void udpSendPicture(int gameno) {
    int x = games[gameno].ball.x;
    int y = games[gameno].ball.y;
    int y0 = games[gameno].boards[0].y;
    int y1 = games[gameno].boards[1].y;
    ////printf("x = %d y = %d y0 = %d y1 = %d\n", x, y, y0, y1);
    std::string picture = std::to_string(x) + " " + std::to_string(y) + " " + std::to_string(y0) + " " + std::to_string(y1);
    const char* buf = (picture + "\n").c_str();
    //printf("player0port = %d\n", games[gameno].addrs[0]->sin_port);
    sendto(udpServerSockfd, buf, strlen(buf), 0, (sockaddr*)games[gameno].addrs[0], sizeof(*games[gameno].addrs[0]));
    //printf("player1port = %d\n", games[gameno].addrs[1]->sin_port);
    sendto(udpServerSockfd, buf, strlen(buf), 0, (sockaddr*)games[gameno].addrs[1], sizeof(*games[gameno].addrs[1]));
    //printf("sendto: %s", buf);
    //delete[] buf;
}

//控制小球移动的线程行为
void* ballMoveThreadFun(void* argsVoid)
{
    while (1) {
        timeval beginTime;
        gettimeofday(&beginTime, nullptr);
        for (int i = 0;i < GAME_N;i++) {
            if (games[i].playerNum == 2 && games[i].ready[0] && games[i].ready[1]) {
                ballMove(i);
                udpSendPicture(i);
                if (games[i].ball.alive == 0) {
                    games[i].playerNum = 0;
                    delete games[i].addrs[0];
                    delete games[i].addrs[1];
                }
            }
        }
        timeval endTime;
        gettimeofday(&endTime, nullptr);
        int us = 1000000;
        int processUs = endTime.tv_sec * us + endTime.tv_usec - (beginTime.tv_sec * us + beginTime.tv_usec);
        int sleepUs = BALL_SLEEP - processUs;
        if (sleepUs > 0) {
            usleep(sleepUs);
        }
    }
}

//public:

void aioReadCompletionHandler(int sig, siginfo_t* info, void* ucontext) {
    //用来获取读aiocb结构的指针
    struct aiocb* prd;
    int ret;
    prd = (struct aiocb*)info->si_value.sival_ptr;

    //判断请求是否成功
    if (aio_error(prd) == 0)
    {
        //获取返回值
        ret = aio_return(prd);
        char* request = (char*)prd->aio_buf;
        request[ret] = '\0';
        //printf("aio_read OK request:%s", request);
        handle_request(prd->aio_fildes, request);
        delete[] request;
        delete prd;
        ////printf("读返回值为:%d\n", ret);
    }

}
int listenfd;

void* IOServerRun(void* args) {
    fd_set fdset;
    //printf("listenfd = %d udpFd = %d maxFd = %d\n", listenfd, udpServerSockfd, maxfd);
    while (true) {
        FD_ZERO(&fdset);
        FD_SET(listenfd, &fdset);
        FD_SET(udpServerSockfd, &fdset);
        int maxfd = listenfd;
        if (udpServerSockfd > maxfd) {
            maxfd = udpServerSockfd;
        }
        int nready = select(maxfd + 2, &fdset, NULL, NULL, NULL);
        //printf("nready = %d\n", nready);
        if (nready < 0) {
            if (errno == EINTR) {
                continue;
            }
            else {
                perror("accept");
                return nullptr;
            }
        }
        while (nready--) {
            //tcp连接进入
            if (FD_ISSET(listenfd, &fdset)) {
                requestCnt++;
                sockaddr_in* newSockaddrIn = new sockaddr_in();
                socklen_t* len = new socklen_t(sizeof(*newSockaddrIn));
                int inSockFd = accept(listenfd, (sockaddr*)newSockaddrIn, len);
                //setnonblocking(inSockFd);
                if (inSockFd < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    else {
                        perror("accept");
                        return nullptr;
                    }
                }
                //sockaddrs[inSockFd] = newSockaddrIn;
                printf("tcp port =  %d\n", newSockaddrIn->sin_port);
                //填充aiocb的基本信息
                aiocb* inSockCb = new aiocb();
                bzero(inSockCb, sizeof(*inSockCb));
                inSockCb->aio_fildes = inSockFd;
                inSockCb->aio_buf = new char[BUFFER_SIZE];
                inSockCb->aio_nbytes = BUFFER_SIZE;
                inSockCb->aio_offset = 0;
                //填充aiocb中有关回调通知的结构体sigevent
                inSockCb->aio_sigevent.sigev_notify = SIGEV_SIGNAL;//使用信号回调通知
                inSockCb->aio_sigevent.sigev_signo = I_SIGNAL;//设置返回信号
                //inSockCb->aio_sigevent.sigev_notify_attributes = nullptr;//使用默认属性
                inSockCb->aio_sigevent.sigev_value.sival_ptr = inSockCb;//在aiocb控制块中加入自己的位置指针

                //printf("begin aio_read:%d\n", inSockFd);
                int ret = aio_read(inSockCb);
                if (ret < 0) { perror("aio_read");return nullptr; }
            }
            //udp消息进入
            if (FD_ISSET(udpServerSockfd, &fdset)) {
                //printf("udp in\n");
                requestCnt++;
                char buf[BUFFER_SIZE];
                sockaddr_in* sockAddrClientUdp = new sockaddr_in();
                socklen_t len = sizeof(*sockAddrClientUdp);
                int n = recvfrom(udpServerSockfd, buf, BUFFER_SIZE, 0, (sockaddr*)sockAddrClientUdp, &len);
                buf[n] = '\0';
                printf("recvfrom port = %d : %s", sockAddrClientUdp->sin_port, buf);
                handle_udpRequest(udpServerSockfd, sockAddrClientUdp, &len, buf);
                //printf("udp port =  %d\n", newSockaddrIn->sin_port);
            }
        }
        if (requestCnt % 1000 == 0) {
            printf("requestCnt = %d\n", requestCnt);
        }
    }
}

int run()
{
    struct sockaddr_in addr;

    //获取并注册监听tcp链接的socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listenfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind1");
        exit(1);
    }
    listen(listenfd, 1);
    if ((udpServerSockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    //获取并注册发送udp数据报的socket

    //服务器网络信息结构体
    struct sockaddr_in udpServerAddr;
    socklen_t addrlen = sizeof(udpServerAddr);
    udpServerAddr.sin_family = AF_INET;
    udpServerAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    udpServerAddr.sin_port = htons(UDP_PORT);
    if (bind(udpServerSockfd, (sockaddr*)&udpServerAddr, addrlen) < 0) {
        perror("bind");
        exit(1);
    }
    // 注册read完成信号处理函数
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = aioReadCompletionHandler;
    sigaction(I_SIGNAL, &sa, NULL);
    // 注册write完成信号处理函数
    struct sigaction wsa;
    sigemptyset(&wsa.sa_mask);
    wsa.sa_flags = SA_SIGINFO;
    wsa.sa_sigaction = aioWriteCompletionHandle;
    sigaction(O_SIGNAL, &wsa, NULL);

    //启动两个控制游戏进程的线程
    //启动小球移动线程
    pthread_t controllBallThread;
    pthread_create(&(controllBallThread), nullptr, ballMoveThreadFun, nullptr);
    //主线程进入IO处理状态
    printf("server start success\n");
    IOServerRun(nullptr);
    return 0;
}
//};

int main()
{
    return run();
}

