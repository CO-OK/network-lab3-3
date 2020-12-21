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
#include"msghdr.h"

int check_hdr_END(char c);
int check_hdr_SYN(char c);
int check_hdr_ACK(char c);
int check_hdr_FLG(char c);
int check_hdr_FIN(char c);
int make_dgram_server_socket(int PortNum,int QueueNum);
int get_internet_address(char*,int,int*,struct sockaddr_in*);
void say_who_called(struct sockaddr_in *);
void clear_buf(char *buf);
void delay(unsigned i);
int switch_to_state0();
int switch_to_state1();
void shake_hand();
void say_goodbye();
int make_sum( unsigned char * buf,int sum);
int check_sum(unsigned char* buf,int sum);
int Sendto(int fd,  void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len,unsigned sum,int PRO);
void ALRM_handler();
int set_timer(int n_msecs);

struct itimerval time_set;
timer_t* time_fd;
int is_recv;
unsigned sum=0;
int port;
int sock;
int fd;
struct sockaddr_in saddr;
unsigned char buf_send[buf_len];
unsigned char buf_recv[buf_len];
socklen_t saddrlen;
size_t msglen;
int nchars;
unsigned long count=0;
unsigned long timecount=0;
unsigned num=0;
int main(int ac,char*av[])
{   
    count=-1;
    timecount=0;
    is_recv=0;
    signal(SIGALRM,ALRM_handler);
    //先打开文件
    //检测参数是否合法
    if(ac==1||(port=atoi(av[1]))<=0)
    {
        fprintf(stderr,"usage: dgrecv portnumber\n");
        exit(1);
    }
    //创建服务器套接字
    if((sock=make_dgram_server_socket(port,5))==-1)
        oops("can not make socket",2);
    
    saddrlen=sizeof(saddr);
   
    shake_hand();
    fd=open(av[2],O_RDONLY);
    //prev_send先发送一个来开启循环
    nchars=read(fd,buf_send+offset,buf_len-offset);
    Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,CLR);
    //开始计时
    set_timer(1);
    count=0;
    printf("send pkt %d with ack=0\n",num);
    num++;
    while(1)
    {
        if(switch_to_state0()==0)
            break;
        if(switch_to_state1()==0)
            break;
    }
    printf("done!\n");
    close(fd);
    printf("take time %ld\n",count);
    return 0;
}

int switch_to_state0()
{
    while(1)
    {
        msglen=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr,&saddrlen);
        is_recv=1;     
        if((check_hdr_ACK(buf_recv[0])&&check_hdr_FLG(buf_recv[0]))||!check_sum(buf_recv,sum)/*buf_recv[0]=='1'*/)
        {
            //重发
            lseek(fd,-1*nchars,SEEK_CUR);
            nchars=read(fd,buf_send+offset,buf_len-offset);
            Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,CLR);
            is_recv=0;
            printf("state1: client ack is 1,expet 0     current num=%d\n",num);
            continue;
        }
        else if(check_hdr_ACK(buf_recv[0])&&!check_hdr_FLG(buf_recv[0])/*buf_recv[0]=='0'*/)
        {
            printf("state1: client ack is 0,correct     current num=%d\n",num);
            break;
        }     
    }
    //发送下一个
    nchars=read(fd,buf_send+offset,buf_len-offset);
    if(nchars==0)
    {
 
        Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,END);
        is_recv=0;
        return 0;//此处将来可能会有计时器
    }
    Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,FLG);
    is_recv=0;
    set_timer(1);
    printf("state1 send pkt %d with ack=1\n",num);
    num++;
    return nchars;      
}

int switch_to_state1()
{
    while(1)
    {
        msglen=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr,&saddrlen);
        is_recv=1;
        if((check_hdr_ACK(buf_recv[0])&&!check_hdr_FLG(buf_recv[0]))||!check_sum(buf_recv,sum)/*buf_recv[0]=='0'*/)
        {
            printf("state2: client ack is 0,expect 1     current num=%d\n",num);
            //重发
            lseek(fd,-1*nchars,SEEK_CUR);
            nchars=read(fd,buf_send+offset,buf_len-offset);
            Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,FLG);
            is_recv=0;
            continue;
        }
        else if(check_hdr_ACK(buf_recv[0])&&check_hdr_FLG(buf_recv[0])/*buf_recv[0]=='1'*/)
        {
            printf("state2: client ack is 1,correct     current num=%d\n",num);
            break;
        }     
    }
    nchars=read(fd,buf_send+offset,buf_len-offset);
    if(nchars==0)
    {
        Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,END);
        is_recv=0;
        return 0;//如果结束信息收不到，应该还需要一个计时器
    }
    Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,CLR);
    is_recv=0;
    set_timer(1);
    printf("state 0 send pkt %d with ack=0\n",num);
    num++;
    return nchars;
}

void shake_hand()
{
     //服务器端的握手
    while(1)
    {
        msglen=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr,&saddrlen);
        if(check_hdr_SYN(buf_recv[0]))
        {

            Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,SYN|ACK);
            break;
        }
    }
    while(1)
    {
        msglen=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr,&saddrlen);
        if(check_hdr_ACK(buf_recv[0])&&check_sum(buf_recv,sum))
            break;
    }
}


int set_timer(int msecs)//毫秒
{   
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
    if(count>=0)
        count++;

    timecount++;
    if(is_recv==0&&timecount==500)
    {
        lseek(fd,-1*nchars,SEEK_CUR);
        nchars=read(fd,buf_send+offset,buf_len-offset);
        Sendto(sock,buf_send, buf_len, 0, (struct sockaddr*)&saddr,saddrlen,sum,FLG);
        is_recv=0;
        timecount=0;
        set_timer(1);
        printf("time out, resend\n");
    } 
    if(timecount==500)
        timecount=0;
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
