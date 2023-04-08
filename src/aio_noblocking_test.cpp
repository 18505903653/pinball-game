// my_aio_read.c
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
// 需要包含 aio.h 文件
#include <aio.h>
#include <strings.h>
#include <errno.h>
#include <string.h>

#define ERR_EXIT(msg) do { perror(msg); exit(1); } while(0)

int main() {
    int fd, ret;
    char buf[64];
    // 定义一个异步控制块结构体，不懂没关系，不用管
    struct aiocb my_aiocb;

    // 初始化
    bzero((char*)&my_aiocb, sizeof(struct aiocb));

    my_aiocb.aio_buf = buf; // 告诉内核，有数据了就放这儿
    my_aiocb.aio_fildes = STDIN_FILENO; // 告诉内核，想从标准输入读数据
    my_aiocb.aio_nbytes = 64; // 告诉内核，缓冲区大小只有 64
    my_aiocb.aio_offset = 0; // 告诉内核，从偏移为 0 的地方开始读

    // 发起异步读操作，立即返回。你并不知道何时 buf 中会有数据
    ret = aio_read(&my_aiocb);
    if (ret < 0) ERR_EXIT("aio_read");

    // 不断的检查异步读的状态，如果返回 EINPROGRESS，说明异步读还没完成
    // 轮询检查状态是一种很笨的方式，其实可以让操作系统用信号的方式来通知，或者让操作系统完成读后主动创建一个线程执行。
    while (aio_error(&my_aiocb) == EINPROGRESS) {
        write(STDOUT_FILENO, ".", 1);
        sleep(1);
    }

    // 打印缓冲区内容，你并不知道内核是什么时候将缓冲区中的 hello 复制到你的 buf 中的。
    printf("len = %d content: %s\n", (int)strlen(buf), buf);

    return 0;
}
