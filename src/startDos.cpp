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
#include <aio.h>
#include <signal.h>

static const int GAME_N = 1e5 + 10;
static const int SLEEP_US = 10000;
static const int BALL_SLEEP = 20;
static const int BOARD_SLEEP = 5;
static const int REFRESH_SLEEP = 3;
static const int BUFFER_SIZE = 32;
int sum = 0;
struct sockaddr_in addr;//服务端的地址

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


void aioWriteCompletionHandle(sigval_t sigval) {
    //用来获取读aiocb结构的指针
    aiocb* prd;
    int ret;
    prd = (struct aiocb*)sigval.sival_ptr;
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
    cb->aio_sigevent.sigev_notify = SIGEV_THREAD;//使用线程回调通知
    cb->aio_sigevent.sigev_notify_function = aioWriteCompletionHandle;//设置回调函数
    cb->aio_sigevent.sigev_notify_attributes = nullptr;//使用默认属性
    cb->aio_sigevent.sigev_value.sival_ptr = cb;//在aiocb控制块中加入自己的位置指针
    printf("begin aio_write fd = %d,buf = %s", fd, buf);
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

int sendPicture(int gameNo) {
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
    std::string message = "start";
    //message = message + std::to_string(gameNo);
    writeString(sockfd, message);
    char buf[32];
    int n = read(sockfd, buf, sizeof(buf));
    buf[n] = '\0';
    printf("%s", buf);
    close(sockfd);
    return 0;
}

void* run(void* args) {
    struct timeval startTime;
    gettimeofday(&startTime, NULL);
    int maxPing = 0;
    int cnt = 0;
    int sumPing = 0;
    while (true) {
        struct timeval time[2];
        /* 获取时间，理论到us */
        gettimeofday(&time[0], NULL);
        //printf("s: %ld, ms: %ld\n", time.tv_sec, (time.tv_sec * 1000 + time.tv_usec / 1000));
        int before = (time[0].tv_sec * 1000 + time[0].tv_usec / 1000);
        sendPicture(0);
        /* 重新获取 */
        gettimeofday(&time[1], NULL);
        int after = (time[1].tv_sec * 1000 + time[1].tv_usec / 1000);
        int ping = after - before;
        sumPing += ping;
        maxPing = ping > maxPing ? ping : maxPing;

        //printf("ping: %d ms\n", ping);
        cnt++;
        if (time[1].tv_sec - startTime.tv_sec >= 600) {
            break;
        }
        //printf("s: %ld, ms: %ld\n", time.tv_sec, (time.tv_sec * 1000 + time.tv_usec / 1000));
    }
    sum += cnt;
    printf("maxPing = %d avgPing = %d cnt = %d\n", maxPing, sumPing / cnt, cnt);
}

int main() {
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // for (int i = 0;i < 5;i++) {
    //     pthread_t* p = new pthread_t();
    //     pthread_create(p, nullptr, run, nullptr);
    // }
    run(nullptr);
    // sleep(13);
    printf("sum = %d\n", sum);
    return 0;
}