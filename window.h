#include"msghdr.h"
#include<sys/socket.h>
#include<stdlib.h>
#include<stdio.h>
#define time_table_num 4 //TimeTable的大小,也是窗口大小
#define window_num 5 //窗口大小
struct WindowItem{
    /*
        窗口中的一个单窗口
    */
    unsigned char recv_buf[buf_len];
    unsigned char send_buf[buf_len];
    struct WindowItem* next;//连接其它单窗口的指针
    int pkg_num;//这个单窗口负责发送的包的序号
    int pkg_size;//这个单窗口负责发送的包的大小
};

struct ServerWindow{
    /*
        窗口结构
    */
    int size;//窗口中单窗口数目
    int next_window_size;//用于拥塞控制
    struct WindowItem* head;//base
    struct WindowItem* tail;//尾指针
    struct WindowItem* next_seq_num;//下一个将要使用的单窗口

};
int MakeServerWindow(struct ServerWindow*  window,int n);//初始化窗口
int DeleteWindow(struct ServerWindow*  window);//删除窗口
int IncreaseWindow(struct ServerWindow*  window,int n);//增加窗口,n为要增加的数量
void PrintWindow(struct ServerWindow*  window);//调试用
int FindRestWindow(struct ServerWindow*  window);//返回剩余空闲窗口数
struct thread_item{
    /*
        这个结构体存储了一个线程所必需的各种变量
    */
    socklen_t* saddrlen;
    struct ServerWindow *window;//窗口
    struct sockaddr_in* saddr;
    pthread_t id;//线程表述符
    char* file_name;//文件名
    int timer_stop;//计时器是否需要停止
    int count;//计时器需要用到的计数值
    int pos;//在TimeTable中的位置，方便删除，提高性能
    int sock;//套接字描述符
    int fd;//文件描述符
    int port;//端口
    int port_index;
    int end_count;//当客户端发送的END丢失时，服务器会重发最后几个包，
                  //为了避免一直重复发送，发送10次后就结束，这个值用来计数
    int shake_hand_done;//标志了握手的三个阶段
    int pkg_num;//序列号
    int break_flag;//循环跳出标志
    int control_state;//拥塞控制的三个状态 0为慢启动，1为拥塞避免，2为快速恢复
    int ssthresh;//拥塞控制的窗口阈值
    int dupACKcount;//重复ACK计数
    int MSS;//最大段长度
    int lastACK;//上一个的seq_num
    int currentACK;//现在收到的seq_num
    int resize_window;//当这个值为1时，不能发送数据包，因为窗口大小在减小
    int dupACKflag;//重复ack标志，是一个互斥锁
    unsigned long t;
};

struct TimeTableItem
{
    /*
        TimeTable的表项
    */
    int valid;//表示这一项是否有效（被占用）
    struct thread_item* item;//指向线程结构体
};
struct TimeTable
{
    /*
        主要目的是方便实现多个线程的计时器
    */
    struct TimeTableItem table[time_table_num];
};
int find_next_pos(struct TimeTable* table);//找到表中下一个有效位置
//将一个thread_item*item加入TimeTable
int TimeTableInsert(struct TimeTable* time_table,struct thread_item*item,int pos);
struct SockPort
{
    int port;
    int valid;
};

