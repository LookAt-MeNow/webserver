#ifndef _EPOLL_H_
#define _EPOLL_H_

#include <sys/epoll.h>
#include <fcntl.h>
#include <vector>
#include <unistd.h>

//非阻塞模式、内核事件表注册事件、删除事件、重置EPOLLONESHOT事件
class epoll{
public:
    epoll();
    ~epoll();
    //非阻塞模式
    int setnonblocking(int fd);
    //内核事件表注册事件
    void addfd(int epollfd, int fd, bool oneshot); //oneshot表示是否注册EPOLLONESHOT事件
    //删除事件
    void removefd(int epollfd, int fd); 
    //重置EPOLLONESHOT事件
    void modfd(int epollfd, int fd, int ev);
private:
    int epollfd; //epoll文件描述符
    std::vector<struct epoll_event> events; //事件数组
};

int epoll::setnonblocking(int fd){
    int old_flag = fcntl(fd, F_GETFL); //获取文件描述符的状态标志
    int new_flag = old_flag | O_NONBLOCK; //用或运算，设置非阻塞模式
    fcntl(fd, F_SETFL, new_flag);
    return old_flag; // 操作完成后返回原来的状态标志
}

void epoll::addfd(int epollfd,int fd,bool oneshot) { //参数: epoll文件描述符、文件描述符、是否注册EPOLLONESHOT事件
    struct epoll_event event; //事件结构体
    event.data.fd = fd; //事件的文件描述符

#ifdef ET //边缘触发模式
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; //监听读事件、边缘触发模式、对端关闭连接或写操做
#endif
#ifdef LT //水平触发模式
        event.events = EPOLLIN | EPOLLRDHUP; //监听读事件、对端关闭连接或写操做
#endif
    //单次触发模式
    if (oneshot) {
        event.events |= EPOLLONESHOT; //添加EPOLLONESHOT事件
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event); //注册时间
    setnonblocking(fd); //设置非阻塞模式
}

void epoll::removefd(int epollfd,int fd) {
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0); //删除事件
    close(fd);  //关闭文件描述符
}

void epoll::modfd(int epollfd,int fd,int ev) { //重置EPOLLONESHOT事件,因为EPOLLONESHOT事件只能被触发一次，再次触发需要重置，ev表示事件类型
    struct epoll_event event;
    event.data.fd = fd;
#ifdef ET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef LT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event); //重置事件

}

#endif