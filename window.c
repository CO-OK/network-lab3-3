#include"window.h"
#include <stdlib.h>
int MakeServerWindow(struct ServerWindow* window,int n)
{
    /*
        创建一个窗口，是一个大小为n的循环链表
    */
    window->size=n;
    struct WindowItem*tmp;
    tmp=window->head=malloc(sizeof(struct WindowItem));
    for(int i=0;i<n-1;i++)
    {
        tmp->next=(struct WindowItem*)malloc(sizeof(struct WindowItem));
        window->tail=tmp;
        tmp=tmp->next;
    }
    window->tail=tmp;
    window->tail->next=window->head;
    window->next_seq_num=window->head; 
    return 0;
}
int DeleteWindow(struct ServerWindow*  window)
{
    /*
        释放窗口
    */
    struct WindowItem*tmp=window->head;
    struct WindowItem*tmp_next=window->head;
    for(int i=0;i<window->size;i++)
    {
        tmp=tmp_next;
        tmp_next=tmp->next;
        free(tmp);
    }
    return window->size;
}
int IncreaseWindow(struct ServerWindow*  window,int n)//增加窗口,n为要增加的数量
{
    //首先构造长度为n的链表,head=tmp_head tail=tmp_tail
    struct WindowItem*tmp_head=(struct WindowItem*)malloc(sizeof(struct WindowItem));
    struct WindowItem*tmp=tmp_head;
    struct WindowItem*tmp_tail;
    tmp_head->pkg_num=-1;
    for(int i=0;i<n-1;i++)
    {
        tmp->next=(struct WindowItem*)malloc(sizeof(struct WindowItem));
        tmp->next->pkg_num=-1;
        tmp_tail=tmp;
        tmp=tmp->next;
    }
    tmp_tail=tmp;
    
    //将其加入原来的链表
    window->tail->next=tmp_head;
    tmp_tail->next=window->head;
    window->tail=tmp_tail;
    window->size=window->size+n;
    return 0;
}
void PrintWindow(struct ServerWindow*  window)
{
    int i=0;
    struct WindowItem*tmp=window->head;
    for(;i<100;i++)
        tmp=tmp->next;
    printf("window:%d\n",i);
}
int find_next_pos(struct TimeTable* table)
{
    /*
        找到TimeTable中一个还未被使用的位置的序号并返回，没有则返回-1
    */
    for(int i=0;i<time_table_num;i++)
    {
        if(table->table[i].valid==1)
            return i;
    }
    return -1;
}
int TimeTableInsert(struct TimeTable* time_table,struct thread_item*item,int pos)
{
    /*
        将一个thread_item加入到TimeTable中
    */
    time_table->table[pos].item=item;
    time_table->table[pos].item->timer_stop=1;
    time_table->table[pos].item->count=0;
    time_table->table[pos].valid=0;
    item->pos=pos;
    return 0;
}