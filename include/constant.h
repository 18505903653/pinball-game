#ifndef _CONSTANT_
#define _CONSTANT_

#define I_SIGNAL SIGUSR1
#define O_SIGNAL SIGUSR2
//服务器IP地址
static const char SERVER_IP[] = "192.168.11.129";
//服务器端口号
static const int TCP_PORT = 8080;
static const int UDP_PORT = 8080;
//游戏画面宽度
static const int MAXX = 20;
//游戏画面长度
static const int MAXY = 60;
//木板长度
static const int BOARD_LENGTH = 20;
//static const int SLEEP_US = 10000;
//小球移动速度限制 0.2秒移动一次
static const int BALL_SLEEP = 200000;
//木板移动速度限制 0.05秒移动一次
static const int BOARD_SLEEP = 50000;
//服务器同时最大游戏对局数量
static const int GAME_N = 30000;
//小球字符
static const char BALL_CHAR = 'o';
//墙壁字符
static const char WALL_CHAR = '|';
//木板字符
static const char BOARD_CHAR = '=';
static const int LEFT = -1;
static const int RIGHT = 1;
static const int UP = -1;
static const int DOWN = 1;
static const int AIO_LIST_SIZE = 16;
static const int EVENT_MAX_COUNT = 16;
//消息缓冲区大小
static const int BUFFER_SIZE = 32;
//应用层协议信息
static const char START[] = "start";
static const char READY[] = "ready";
static const char MOVE[] = "move";
#endif