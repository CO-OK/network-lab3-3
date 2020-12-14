#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/time.h>
#include<signal.h>
#include<pthread.h>
#include"msghdr.h"
#include"window.h"

/*
以下四个函数检查信息头，若对应位为1,则返回1
*/
int check_hdr_END(char c);
int check_hdr_SYN(char c);
int check_hdr_ACK(char c);
int check_hdr_FIN(char c);
void make_hdr(unsigned char*buf,int PRO);
void clear_buf(unsigned char*buf);
int make_dgram_server_socket(int PortNum,int QueueNum);//将创建服务器套接字的一些函数集成
int get_internet_address(char*,int,int*,struct sockaddr_in*);//这个函数目前并未用到
void say_who_called(struct sockaddr_in *);//这个函数目前并未用到
void shake_hand(struct thread_item* item);//握手函数
void say_goodbye();//这个函数目前并未用到
int make_sum( unsigned char * buf,int len);//生成头部校验和
int check_sum(unsigned char* buf,int len);//检查校验和
int Sendto(int fd,  void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len,int PRO);//封装sendto、make_sum、make_hdr
void ALRM_handler();//计时器到时的处理函数
int set_timer(int n_msecs);//设置计时器
int CheckArg(int ac,char*av[]);//检查参数
void  thread_send(void *arg);//线程发送函数
int lab3_2_Sendto(int fd, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len,int PRO,struct WindowItem* item,int pkg_num);//封装sendto、make_sum、make_hdr、make_pkg_num
int read_pkg_num(unsigned char* buf);//读取数据包的序号
int make_thread_item(struct thread_item* item);//初始化一个线程结构体
int find_next_port(int len,int*index);//找到当前服务器的一个可用端口
int IncreaseWindow(struct ServerWindow*  window,int n);//增加窗口大小
void DecreaseWindow(struct ServerWindow*  window,int n);
void PrintWindow(struct ServerWindow*  window);//调试用
void send_handle(struct thread_item*item);//发送包控制函数
void recv_handle(struct thread_item*item);//接收包控制函数
void release_res(struct thread_item*item);//释放资源
void new_ACK_handle(struct thread_item*item);//拥塞控制收到新ACK后如何处理
void time_out_handle(struct thread_item*item);//超时后如何处理
struct itimerval time_set;//设置总计时器相关
struct TimeTable time_table;//一个存储了所有计时器的全局表
struct SockPort* port_list;//服务器端口列表
int main(int ac,char*av[])
{
    signal(SIGALRM,ALRM_handler);//为计时器设置好处理函数
    port_list=malloc(sizeof(struct SockPort)*(ac-2));//初始化端口列表
    for(int i=0;i<(ac-2);i++)//将端口全部置为可用状态valid=1
    {
        port_list[i].port=atoi(av[i+1]);
        port_list[i].valid=1;
    }
    for(int i=0;i<time_table_num;i++)//初始化time_table
        time_table.table[i].valid=1;
    set_timer(1);//设置计时器
    int index;//端口索引
    while (1)
    {
        int port;
        if(find_next_pos(&time_table)!=-1&&(port=find_next_port(time_table_num,&index))!=-1)//查看是否有可用的端口号以及计时器表里是否有位置
        {
            int sock;
            printf("port: %d\n",port);
            if((sock=make_dgram_server_socket(port,1))==-1)//创建套接字
                oops("can not make socket",2);
            struct thread_item* item=malloc(sizeof(struct thread_item));//创建一个新的线程结构体，方便传参
            make_thread_item(item);//初始化该结构体
            item->window=malloc(sizeof(struct ServerWindow));//初始化窗口
            item->file_name=av[ac-1];//要打开的文件名
            item->sock=sock;//套接字
            item->port=port;//端口
            item->port_index=index;//保存了所用到的端口在port_list里的索引，方便删除
            item->shake_hand_done=-1;//表示此时还未握手，用来约束计时器
            item->pkg_num=0;//提前将第一个包的序号置0
            item->break_flag=0;//在线程中用到
            item->end_count=-1;//是否启动最后计数
            item->control_state=0;//刚开始是慢启动
            item->resize_window=0;
            item->dupACKflag=0;
            item->ssthresh=15;
            item->lastACK=-1;
            item->send_down=0;
            item->RTT=0;
            item->RTT_COUNT=0;
            item->Average_Throughput_Count=0.0;
            item->Average_Throughput=0;
            //慢启动初始值
            /*code*/
            if((pthread_create(&(item->id),NULL,(void*)&thread_send,(void*)(item)))!=0)
            {
                oops("there is somthing wrong when create a new thread\n",1);
            }
            printf("done!\n");
        }  
    }
    return 0;
}


void shake_hand(struct thread_item* item)
{
     //服务器端的握手
    unsigned char buf_send[buf_len];
    unsigned char buf_recv[buf_len];
    while(1)
    {
        recvfrom(item->sock,buf_recv,buf_len,0,(struct sockaddr*)item->saddr,&(*item->saddrlen));
        if(check_hdr_SYN(buf_recv[0]))
        {
            Sendto(item->sock,buf_send, buf_len, 0, (struct sockaddr*)item->saddr,*item->saddrlen,SYN|ACK);
            //设置计时器
            item->shake_hand_done=0;
            break;
        }
    }
    while(1)
    {
        recvfrom(item->sock,buf_recv,buf_len,0,(struct sockaddr*)item->saddr,&(*item->saddrlen));
        if(check_hdr_ACK(buf_recv[0]))
        //计时器失效
        {
            item->shake_hand_done=1;
            break;
        }
            
    }
    printf("server shake done\n");
}


int set_timer(int msecs)//毫秒
{
    /*
        使用系统调用setitimer
    */
    struct itimerval tmp_time_set;
    long sec,usec;
    sec=msecs/1000;//秒
    usec=(msecs%1000)*1000L;//微秒
    tmp_time_set.it_interval.tv_sec=sec;
    tmp_time_set.it_interval.tv_usec=usec;
    tmp_time_set.it_value.tv_sec=sec;
    tmp_time_set.it_value.tv_usec=usec;
    return setitimer(ITIMER_REAL,&tmp_time_set,&time_set);
}

void ALRM_handler()
{
    /*
        计时器处理函数，由于每个线程有不同的计时器，而一个进程只有一个计时器，故以进程的计时器作为统一
        频率（目前1ms一次），每来一个信号，每个线程的计数值count+1,从而达到多个计时器运行的假象。
    */
    for(int i=0;i<time_table_num;i++)
    {
        if(time_table.table[i].valid==0&&time_table.table[i].item->RTT==1)
            time_table.table[i].item->RTT_COUNT++;
        if(time_table.table[i].valid==0&&time_table.table[i].item->dupACKflag!=1)//只对表中有效的线程执行,并且不处在dupack=3的重置阶段
        {
            time_table.table[i].item->count++;//进行计数自增
            if(time_table.table[i].item->count==250&&time_table.table[i].item->shake_hand_done==1)//对到时的进行重发
            {
                time_table.table[i].item->count=0;//重新计时
                time_out_handle(time_table.table[i].item);
                //计算平均吞吐量
                //double t=(0.75*time_table.table[i].item->window->size*buf_len)/200;

                //printf("Throughput=%fMbps,count=%d\n",t,time_table.table[i].item->RTT_COUNT);
                //time_table.table[i].item->Average_Throughput+=t;
                //time_table.table[i].item->Average_Throughput_Count++;
                /*if(!time_table.table[i].item->timer_stop&&time_table.table[i].item->dupACKflag!=1)//如果计时器没有被停止则超时后进行重传
                {
                    printf("resend port %d\n",time_table.table[i].item->port);
                    //time_out_handle(time_table.table[i].item);
                    //如果客户端最后一次发送过来的END=1的包丢失，服务器自动发送10次（由item->end_count计数）后结束
                    if(time_table.table[i].item->end_count>=0)
                    {
                        time_table.table[i].item->end_count++;
                        struct WindowItem*tmp=time_table.table[i].item->window->head;
                        //重发
                        while(tmp!=time_table.table[i].item->window->next_seq_num)
                        {
                            printf("resend %d\n",tmp->pkg_num);
                            lab3_2_Sendto(time_table.table[i].item->sock, tmp->pkg_size, 0, (struct sockaddr*)time_table.table[i].item->saddr,\
                                            *time_table.table[i].item->saddrlen,tmp->send_buf[0],tmp,tmp->pkg_num);
                            tmp=tmp->next;
                        }
                        if(time_table.table[i].item->end_count>=10)
                        {
                            //结束线程
                            pthread_cancel(time_table.table[i].item->id);
                            release_res(time_table.table[i].item);
                            printf("wrong exit\n");
                        }
                    }
                    else
                    {
                        time_out_handle(time_table.table[i].item);
                    }
                    
                    
                }*/
                
            }
            //若握手时出现丢包
            else if(time_table.table[i].item->shake_hand_done==0&&time_table.table[i].item->count==3000)
            {
    
                printf("shake_hand_done:%d\n",(time_table.table[i].item->shake_hand_done));
                char buf_send[buf_len];
                time_table.table[i].item->count=0;//重新计时
                //重发
                Sendto(time_table.table[i].item->sock,buf_send, buf_len, 0, (struct sockaddr*)time_table.table[i].item->saddr,*time_table.table[i].item->saddrlen,SYN|ACK);
            }
        }
    }
}


void thread_send(void *arg)
{
    int pkg_num=0;
    int nchars=0;
    struct thread_item* item=arg;
    int pos=find_next_pos(&time_table); //找到时间表中一个合适的位置  
    TimeTableInsert(&time_table,item,find_next_pos(&time_table));//将项目加入表项
    MakeServerWindow(item->window, 3);//初始化窗口，慢启动阶段cwnd=1
    shake_hand(item);//握手
    item->fd=open(item->file_name,O_RDONLY); //打开文件
    printf("open\n");  
    //recv
    while(1)
    {
        send_handle(item);
        //item->RTT=1;
        //item->RTT_COUNT=0;
        recv_handle(item);
        if(item->break_flag)
        {
            printf("break\n");
            break;
        }
    }
    //printf("Total Throughput=%f\n",item->Average_Throughput/item->Average_Throughput_Count);
    release_res(item);
    close(item->fd); 
}
void recv_handle(struct thread_item*item)
{
    int nchars=0;
    unsigned char recv[buf_len];
    while(item->window->head!=item->window->next_seq_num)
    {
        nchars=recvfrom(item->sock,recv,buf_len,0,(struct sockaddr*)item->saddr,&(*item->saddrlen));
        printf("port=%d recv pkg %d  head->pkg_num=%d currentACK=%d \n",item->port,read_pkg_num(recv),item->window->head->pkg_num,item->currentACK);
        if(check_sum(recv,nchars))//收到的包校验和是否正确
        {
            item->RTT=0;
            if(check_hdr_END(recv[0]))//收到返回的END则退出
            {
                item->break_flag=1;
                break;
            }
            item->currentACK=read_pkg_num(recv);
            while(item->window->head->pkg_num<=item->currentACK&&item->window->head!=item->window->next_seq_num)
            {
                item->window->head=item->window->head->next;//base后移
                printf("after move back, head->pkg_num=%d\n",read_pkg_num(item->window->head->send_buf));
                item->window->tail=item->window->tail->next;//base后移后多出一个空闲的单窗口
            }     
            if(item->lastACK==item->currentACK)//重复后dupACKcount自增
            {
                if(item->control_state==0||item->control_state==1)//如果在慢启动和拥塞避免状态下，dupack自增
                {
                    item->dupACKcount++;
                }
                else//快速恢复状态则增加窗口
                {
                    IncreaseWindow(item->window,1);
                }
                if(item->dupACKcount==3&&(item->control_state==0||item->control_state==1))
                {
                    item->dupACKflag=1;
                    item->ssthresh=item->window->size/2;//ssthresh=cwnd/2
                    struct WindowItem*tmp=item->window->head;
                    struct WindowItem*tmp1=tmp->next;
                    //回退读指针
                    while(tmp1!=item->window->next_seq_num&&!check_hdr_END(tmp1->send_buf[0]))
                    {
                        lseek(item->fd,-(tmp1->pkg_size-offset),SEEK_CUR);
                        item->pkg_num--;
                        tmp1=tmp1->next;
                    }
                    //开始重整窗口，用一个新的取代旧的
                    tmp1=tmp->next;
                    struct ServerWindow* new_window=malloc(sizeof(struct ServerWindow));//初始化窗口
                    MakeServerWindow(new_window, item->ssthresh+2);//cwnd=ssthresh+2
                    //将需要重发的加入新窗口
                    new_window->tail->next=tmp;
                    tmp->next=new_window->head;
                    new_window->head=tmp;
                    new_window->size++;
                    //将旧窗口窗口数减一，并还原为循环链表
                    item->window->size--;
                    item->window->tail->next=tmp1;
                    item->window->head=tmp1;
                    //删除旧窗口
                    DeleteWindow(item->window);
                    //更新item->window
                    item->window=new_window;
                    //重发head
                    lab3_2_Sendto(item->sock, tmp->pkg_size, 0, (struct sockaddr*)item->saddr,\
                        *item->saddrlen,tmp->send_buf[0],tmp,tmp->pkg_num);
                    item->control_state=2;
                    item->dupACKflag=0;
                }
            }
            else if(!item->resize_window&&item->lastACK<item->currentACK)//如果是新ACK且不是由于超时所造成的新ACK的假象
            {
                item->lastACK=item->currentACK;//更新lastACK
                //item->dupACKcount=0;//更新dupACKcount
                printf("new ack handle\n");
                new_ACK_handle(item);
            }
            if(item->window->head==item->window->next_seq_num)//base=next
            {
                item->timer_stop=1;
                printf("recv break\n");
                break; 
            }    
            else
            {
                //否则重置计时器
                item->timer_stop=0;
                item->count=0;
            }
        }    
    }
}
void send_handle(struct thread_item*item)
{
    int nchars=0;
    while(item->window->tail!=item->window->next_seq_num&&!item->resize_window)//只要next_seq_num不到达窗口末尾且窗口没在重整，就继续发送
    {
        nchars=read(item->fd,item->window->next_seq_num->send_buf+offset,buf_len-offset);//读取文件到数据段
        printf("nchars=%d\n",nchars);
        if(nchars==0)//如果读完，则发送带END的信息
        {
            lab3_2_Sendto(item->sock, nchars+offset, 0, (struct sockaddr*)item->saddr,*item->saddrlen,END,item->window->next_seq_num,item->pkg_num);
            printf("port %d send end pkg %d\n",item->port,item->pkg_num);
            item->count=0;//重新计时
            item->end_count=0;//开始结束时的计数
            item->send_down=1;
            item->window->next_seq_num=item->window->next_seq_num->next;//next_seq_num后移
            printf("port %d server send done\n",item->port);
            break;
        }
        lab3_2_Sendto(item->sock, nchars+offset, 0, (struct sockaddr*)item->saddr,*item->saddrlen,ACK,item->window->next_seq_num,item->pkg_num);
        printf("port %d send pkg %d , len=%d\n",item->port,item->pkg_num,offset+nchars);
        item->pkg_num++;//发送完之后序列号自增
        if(item->window->head==item->window->next_seq_num)
        {
            //如果窗口变为0,开始计时
            item->timer_stop=0;
            item->count=0;
        }      
        item->window->next_seq_num=item->window->next_seq_num->next;//next_seq_num后移
    }
}

void say_goodbye()
{

}
void say_who_called(struct sockaddr_in *addrp)
{
    char    host[BUFSIZ];
    int port;
    get_internet_address(host,BUFSIZ,&port,addrp);
    printf("from: %s: %d\n",host,port);
}
int make_thread_item(struct thread_item* item)
{
    /*
        对线程结构体的成员进行初始化
    */
    item->saddr=malloc(sizeof(struct sockaddr_in));
    item->saddrlen=malloc(sizeof( socklen_t));
    *item->saddrlen=sizeof(*item->saddr);
    return 0;
}

int CheckArg(int ac,char*av[])
{
    if(ac!=2+time_table_num)
        oops("args num error\n",1);
    /*for(int i=1;i<=ac-1;i++)
    {
        for(int j=0;j<strlen(av[i]);j++)
            if(av[i][j]<'0'||av[i][j]>'9')
                oops("wrong port", 2);
    }*/
    return 0;
}

int find_next_port(int len,int*index)
{
    /*
        找到所有端口中的一个可用端口，若都在使用，返回-1
    */
    for(int i=0;i<len;i++)
    {
        if(port_list[i].valid==1)
        {
            printf("index=%d\n",i);
            port_list[i].valid=0;
            *index=i;
            return port_list[i].port;
        }
    }
    return -1;
}
void release_res(struct thread_item*item)
{
    /*
        释放线程所占用的资源
    */
    time_table.table[item->pos].valid=1;
    //关闭文件描述符
    close(time_table.table[item->pos].item->sock);
    //释放端口
    for(int i=0;i<time_table_num;i++)
        if(port_list[i].port==item->port)
            port_list[i].valid=1;
    //释放线程资源
    //窗口释放
    printf("delete window num=%d\n",DeleteWindow(item->window));
    free(item->window);
    printf("thread window deleted\n");
    //删除线程参数结构体
    free(item);
    printf("one thread finished\n");
}
void new_ACK_handle(struct thread_item*item)
{
    if(item->control_state==0)//慢启动
    {
        printf("in new ack state 0\n");
 
        IncreaseWindow(item->window,1);
        item->dupACKcount=0;
        if(item->window->size>=item->ssthresh)
        {
            printf("state 0 switch to state 1\n");
            item->control_state=1;
        }
        return;
    }
    else if(item->control_state==1)//拥塞避免
    {
        printf("in new ack state 1\n");
 
        IncreaseWindow(item->window,1);
        item->dupACKcount=0;
        return;
    }
    else if(item->control_state==2)//快速恢复
    {
        printf("int new ack state 2\n");
        item->window->next_window_size=item->ssthresh;
        /*if(item->window->next_window_size>item->window->size)
        {
            IncreaseWindow(item->window,item->window->next_window_size-item->window->size);
        }
        else if(item->window->next_window_size<item->window->size)
        {
            DecreaseWindow(item->window,item->window->next_window_size-item->window->size);
        }*/
        DecreaseWindow(item->window,item->window->next_window_size-item->window->size);
        item->dupACKcount=0;
        item->control_state=1;
    }
}

void time_out_handle(struct thread_item*item)
{
    //首先重传然后接收，确保所有包都传到后再减小窗口
    item->resize_window=1;
    //cwnd=1;
    //先等待所有包都收到
    /*while(!item->timer_stop)
    {
        printf("resend port %d\n",item->port);
        struct WindowItem*tmp=item->window->head;
        //重发
        while(tmp!=item->window->next_seq_num)
        {
            printf("resend %d\n",tmp->pkg_num);
            lab3_2_Sendto(item->sock, tmp->pkg_size, 0, (struct sockaddr*)item->saddr,\
                            *item->saddrlen,tmp->send_buf[0],tmp,tmp->pkg_num);
            tmp=tmp->next;
        }
        usleep(500000);//还是每隔500ms发
    }
    //窗口调整为1
    DecreaseWindow(item->window, item->window->size-2);*/
    item->ssthresh=item->window->size/2;//ssthresh=cwnd/2
    struct WindowItem*tmp=item->window->head;
    struct WindowItem*tmp1=tmp->next;
    //回退读指针
    while(tmp1!=item->window->next_seq_num&&!check_hdr_END(tmp1->send_buf[0]))
    {
        lseek(item->fd,-(tmp1->pkg_size-offset),SEEK_CUR);
        item->pkg_num--;
        tmp1=tmp1->next;
    }
    //lseek(item->fd,tmp1->pkg_size-offset,SEEK_CUR);
    //开始重整窗口，用一个新的取代旧的
    tmp1=tmp->next;
    struct ServerWindow* new_window=malloc(sizeof(struct ServerWindow));//初始化窗口
    MakeServerWindow(new_window, 2);//cwnd=1
    //将需要重发的加入新窗口
    new_window->tail->next=tmp;
    tmp->next=new_window->head;
    new_window->head=tmp;
    new_window->size++;
    //将旧窗口窗口数减一，并还原为循环链表
    item->window->size--;
    item->window->tail->next=tmp1;
    item->window->head=tmp1;
    //删除旧窗口
    DeleteWindow(item->window);
    //更新item->window
    item->window=new_window;
    //重发head
    printf("resend pkg %d\n",read_pkg_num(tmp->send_buf));
    //char c=tmp->send_buf[0];
    lab3_2_Sendto(item->sock, tmp->pkg_size, 0, (struct sockaddr*)item->saddr,\
        *item->saddrlen,tmp->send_buf[0],tmp,tmp->pkg_num);
    //item->window->next_seq_num=item->window->next_seq_num->next;
    item->control_state=2;
    //make_hdr(tmp->send_buf,c);
    //dupACK=0
    item->dupACKcount=0;
    switch(item->control_state)
    {
        case 0:
            break;
        case 1:
        {
            item->control_state=0;
            printf("1 to 0\n");
            break;
        }
        case 2:
        {
            item->control_state=0;
            printf("2 to 0\n");
            break;
        }
        default:
        {
            printf("shouldn't be here\n");
            break;
        }
    }
    item->resize_window=0;
}