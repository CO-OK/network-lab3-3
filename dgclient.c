/*
用来从客户端接受一个文件
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<unistd.h>
#include"msghdr.h"

int check_hdr_END(char c);
int check_hdr_SYN(char c);
int check_hdr_ACK(char c);
int check_hdr_FLG(char c);
int check_hdr_FIN(char c);
int make_dgram_client_socket();
int make_internet_address(char*,int,struct sockaddr_in*);
int check_ac(int ac,int num);
void clear_buf(char *buf);
void delay(unsigned i);
int switch_to_state0();
int switch_to_state1();
void shake_hand();
void say_goodbye();
int make_sum( unsigned char * buf,int sum);
int check_sum(unsigned char* buf,int sum);
int Sendto(int fd,  void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len,unsigned sum,int PRO);
int sock;
char*msg;
struct sockaddr_in saddr_to,saddr_from;
socklen_t saddrlen;
unsigned char buf_recv[buf_len];
unsigned char buf_send[buf_len];
int fd;
int nchars;
unsigned num=0;
unsigned sum=0;
int main(int ac,char*av[])
{
    
    if(!check_ac(ac,4))
        exit(1);
    if((sock=make_dgram_client_socket())==-1)
    {
        oops("can not make sockt",2);
    }
    make_internet_address(av[1],atoi(av[2]),&saddr_to);
    msg="";
    //客户端三次握手
    shake_hand();
    if((fd=creat(av[3],0644))==-1)
        oops("create file failed",4);
    while(1)
    {
        if(switch_to_state0()==-1)
            break;
        if(switch_to_state1()==-1)
            break;
    }
    close(fd);
    return 0;
}
int switch_to_state0()
{
    while(1)
    {
        nchars=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr_from,&saddrlen);
        if(check_hdr_END(buf_recv[0]))
        {
            printf("state0: recv server ack=2, exit\n");
            return -1;
        }
        if(check_hdr_FLG(buf_recv[0])||!check_sum(buf_recv,sum)/*buf_recv[0]=='1'*/)
        {
            Sendto(sock , buf_send, buf_len, 0, (struct sockaddr*)&saddr_to, sizeof(saddr_to),sum,ACK|FLG);
            printf("state0: server ack is 1,expect 0,resend     current num=%d\n",num);
            continue;
        }
        else if(!check_hdr_FLG(buf_recv[0]))
        {
            if((nchars-offset)!=write(fd, buf_recv+offset, nchars-offset))
                exit(8);
            Sendto(sock , buf_send, buf_len, 0, (struct sockaddr*)&saddr_to, sizeof(saddr_to),sum,ACK);
            printf("state0: server ack is 0,correct     current num=%d\n",num);
            num++;
            break;
        }
    }   
    return 0;
}
int switch_to_state1()
{
    while(1)
    {
        nchars=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr_from,&saddrlen);
        if(check_hdr_END(buf_recv[0]))
        {
            printf("state1: recv server ack=2, exit\n");
            return -1;
        }
        if(!check_hdr_FLG(buf_recv[0])||!check_sum(buf_recv,sum)/*buf_recv[0]=='0'*/)
        {

            Sendto(sock , buf_send, buf_len, 0, (struct sockaddr*)&saddr_to, sizeof(saddr_to),sum,ACK);
            printf("state1: server ack is 0,expect 1,resend 0     cunnent num=%d\n",num);
            continue;
        }
        else if(check_hdr_FLG(buf_recv[0])/*buf_recv[0]=='1'*/)
        {
            if((nchars-offset)!=write(fd, buf_recv+offset, nchars-offset))
                exit(8);
            Sendto(sock , buf_send, buf_len, 0, (struct sockaddr*)&saddr_to, sizeof(saddr_to),sum,ACK|FLG);
            printf("state1: server ack is 1,correct     current num=%d\n",num);
            num++;
            break;
        }
    }   
    return 0;
}
void shake_hand()
{
    Sendto(sock , buf_send, buf_len, 0, (struct sockaddr*)&saddr_to, sizeof(saddr_to),sum,SYN);
    nchars=recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr_from,&saddrlen);
    if(check_hdr_SYN(buf_recv[0])&&check_hdr_ACK(buf_recv[0]))
    {
        Sendto(sock , buf_send, buf_len, 0, (struct sockaddr*)&saddr_to, sizeof(saddr_to),sum,ACK);
    }
    else
    {
        oops("client receive a wrong msg",1);
    }
}





void say_goodbye()
{
    buf_send[0]&=CLR;
    buf_send[0]|=ACK;
    buf_send[0]|=FIN;
    sendto(sock,buf_send,buf_len,0,(struct sockaddr*)&saddr_to,sizeof(saddr_to));
    recvfrom(sock,buf_recv,buf_len,0,(struct sockaddr*)&saddr_from,&saddrlen);
}