#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string>
struct sockaddr_in addr;//服务端的地址
int gameNo, playerNo;
int writeString(int fd, std::string s) {
    char buf[1024];
    for (int i = 0;i < s.length();i++) {
        buf[i] = s[i];
        buf[i + 1];
    }
    buf[s.length()] = '\n';
    buf[s.length() + 1] = '\0';
    return write(fd, buf, strlen(buf));
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
    read(sockfd, buf, sizeof(buf));
    close(sockfd);
    char xStr[4], yStr[4], y0Str[4], y1Str[4];
    sscanf(buf, "%s%s%s%s", xStr, yStr, y0Str, y1Str);
    int x = atoi(xStr);
    int y = atoi(yStr);
    int y0 = atoi(y0Str);
    int y1 = atoi(y1Str);
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
    close(sockfd);
    buf[n] = '\0';
    printf("%s", buf);
    char gameNoStr[10], playerNoStr[4];
    sscanf(buf, "%s%s", gameNoStr, playerNoStr);
    gameNo = atoi(gameNoStr);
    playerNo = atoi(playerNoStr);
    return 0;
}

int main() {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    while (true) {
        char buf[1024] = "picture 0";

        //scanf("%s", buf);
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
        write(sockfd, buf, strlen(buf));
        int n = read(sockfd, buf, sizeof(buf));
        buf[n] = '\0';
        printf("%s\n", buf);

        char xStr[4], yStr[4], y0Str[4], y1Str[4];
        sscanf(buf, "%s%s%s%s", xStr, yStr, y0Str, y1Str);
        printf("%s %s %s %s\n", xStr, yStr, y0Str, y1Str);
        int x = atoi(xStr);
        int y = atoi(yStr);
        int y0 = atoi(y0Str);
        int y1 = atoi(y1Str);
        printf("%d %d %d %d\n", x, y, y0, y1);
        break;
    }
}