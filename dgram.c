#include <stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<netdb.h>
#include<string.h>
#include"msghdr.h"
#define HOSTLEN     256
#ifndef oops
#define oops(m,x)   {perror(m)  ;exit(x)    ;}
#endif
//int make_internet_address();

int make_dgram_server_socket(int PortNum,int QueueNum)
{
    struct sockaddr_in saddr;
    int sock_id;
    if((sock_id=socket(PF_INET,SOCK_DGRAM,0))==-1)
    {
        oops("There is somthing wrong when server make a sock\n",5);
    }
    bzero((void*)&saddr,sizeof(saddr));
    saddr.sin_addr.s_addr=htons(INADDR_ANY);
    saddr.sin_port=htons(PortNum);//端口号
    saddr.sin_family=AF_INET;//地址族
    if(bind(sock_id,(struct sockaddr*)&saddr,sizeof(saddr))!=0)//将sock与saddr绑定
    {
        oops("There is somthing wrong when server bind a sock\n",5);
    }
    return sock_id;
}
int get_internet_address(char*host,int len,int*portp,struct sockaddr_in *addrp)
{
    strncpy(host,inet_ntoa(addrp->sin_addr),len);
    *portp=ntohs(addrp->sin_port);
    return 0;
}

int make_dgram_client_socket()
{
    return socket(PF_INET,SOCK_DGRAM,0);
}

int make_internet_address(char*ip,int port,struct sockaddr_in *addrp)
{
    bzero((void*)addrp,sizeof(*addrp));//先清空saddr
    addrp->sin_addr.s_addr=inet_addr(ip);
    addrp->sin_port=htons(port);
    addrp->sin_family=AF_INET;
    return 0;
}

int check_ac(int ac,int num)
{
    if(ac==num)
        return 1;
    else
    {
        fprintf(stderr,"agr number is not correct\n");
        return 0;
    }
}

void clear_buf(char*buf)
{
    for(int i=0;i<BUFSIZ+1;i++)
        buf[i]='\0';
}

void delay(unsigned i)
{
    for(int j=0;j<i;j++);
}

int check_sum( unsigned char* buf,int sum)
{
    sum=0;
    for(int i=0;i<buf_len;i++)
    {
        if(i!=1&&i!=2&&i!=3)
        {
            sum+=buf[i];
        }
    }
    unsigned char a,b,c;
    a=sum;
    b=sum>>8;
    c=sum>>16;
    if(a+buf[1]==255&&b+buf[2]==255&&c+buf[3]==255)
        return 1;
    return 0;
}

int make_sum(unsigned char * buf,int sum)
{
    sum=0;
    for(int i=0;i<buf_len;i++)
    {
        //printf("%d\n",buf[i]);
        if(i!=1&&i!=2&&i!=3)
            sum+=buf[i];
        else
        {
            buf[i]=0;
        }
    }
    buf[1]=~sum;
    buf[2]=~(sum>>8);
    buf[3]=~(sum>>16);
    return sum;
}

void make_hdr(char*buf,int PRO)
{
    /*
    根据PRO，将相应的属性位置1
    */
    buf[0]&=CLR;
    buf[0]|=PRO;
}


int Sendto(int fd,  void *buf, size_t n, int flags, const struct sockaddr *addr, socklen_t addr_len,unsigned sum,int PRO)
{
    make_hdr(buf,PRO);
    make_sum(buf,sum);
    return sendto(fd,buf,n,flags,addr,addr_len);
}


/*
检查相应属性位是否为1,是1则返回1
*/
int check_hdr_SYN(char c)
{
    return ((c&SYN)==64);
}
int check_hdr_ACK(char c)
{
    return ((c&ACK)==128);
}
int check_hdr_FLG(char c)
{
    return((c&FLG)==32);
}
int check_hdr_FIN(char c)
{
    return 0;
}
int check_hdr_END(char c)
{
    return ((c&END)==16);
}