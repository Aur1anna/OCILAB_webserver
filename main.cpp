#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <cstring>

#include "MemoryPool.h"
#include "./Timer/lst_timer.h"
#include "timer_conn.h"
#include "locker.h"
#include "./ThreadPool/threadpool.h"
#include "./mysql_connect/mysql_conn.h"
#include "./http/http_conn.h"


using namespace My_memoryPool;

#define MAX_FD 65536   // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000  // 监听的最大的事件数量
#define MAX_SQL_NUM 8  // 数据库连接池的最大连接数
#define MAX_THREAD_NUM 8     // 线程池的最大线程数
#define MAX_REQUESTS_NUM 10000  // 线程池的最大请求数
constexpr int TIMESLOT = 5;  // 定时器的时间间隔

// 添加文件描述符到epoll的函数：
extern void addfd( int epollfd, int fd, bool one_shot );
// 从epoll中删除文件描述符的函数：
extern void removefd( int epollfd, int fd );





int main( int argc, char* argv[] ) {
    
    if( argc <= 1 ) {
        printf( "usage: %s port_number\n", basename(argv[0]));
        return 1;
    }

    // 获取端口号
    int port = atoi( argv[1] );


    printf("port is %d\n", port);
    // 创建一个数组 用于保存所有的客户端信息
    http_conn* users = new http_conn[ MAX_FD ];

    printf("users set\n");


/**********************************初始化数据库连接池**********************************/
    
    string user = "root";
    string passwd = "QQgqs1120!";
    string databasename = "webserver_register";

    connection_pool *m_connPool;    //单例连接池
    m_connPool = connection_pool::GetInstance();

    printf("m_connPool address: %p\n", m_connPool);  // 输出单例地址

    m_connPool->init("localhost", user, passwd, databasename, 3306, MAX_SQL_NUM);

    printf("Database connect to root success.\n");

    // http_conn* users = new http_conn[ MAX_FD ];
    //初始化数据库读取表
    users->initmysql_result(m_connPool);

/************************************初始化线程池*************************************/
    // 创建和初始化线程池
    threadpool< http_conn >* pool = NULL;
    try {
        pool = new threadpool<http_conn>(MAX_THREAD_NUM, MAX_REQUESTS_NUM, m_connPool);
    } catch( ... ) {
        return 1;
    }

    
/**********************************服务器开一个监听socket******************************/

    // 创建监听套接字,socket返回整数描述符
    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );

    // 设置socket为端口复用
                //对一个套接字进行配置并将其绑定到一个特定的IP地址和端口上，使得该套接字可以被其他进程复用。
    int reuse = 1;
    setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );  //SO_REUSEADDR选项，允许同一地址的端口可以被快速重新绑定，reuse设置为1表示允许端口复用。

    //设置ip地址端口号：address
    int ret = 0;
                //TCP/IP协议默认配置：
    struct sockaddr_in address;             //赋给addressIP地址和端口号。sockaddr_in是一个用于IPv4通信的地址结构体，包含服务器的IP地址和端口号等信息。
    address.sin_addr.s_addr = INADDR_ANY;   //设置addressIP地址为INADDR_ANY，表示可以接收来自任何IP地址的连接。
    address.sin_family = AF_INET;           //设置addressIP地址的类型为AF_INET，表示为IPv4地址。
    address.sin_port = htons( port );       //设置addressIP地址的端口号。htons函数将主机字节顺序的端口号转换为网络字节顺序。

    // 绑定address到listenfd
    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );

    // 监听，listen 函数用于将套接字转换为被动套接字（即监听套接字），使它能够接受来自客户端的连接请求。
    ret = listen( listenfd, 5 );                //5表示可以在队列中排队等待的最大连接数。当多个客户端同时发起连接请求时，服务器会将这些请求排队。如果队列已满，新的连接请求会被拒绝。




/************************************epoll监视socket**********************************/

    // 创建epoll对象，和事件数组，添加
    epoll_event events[ MAX_EVENT_NUMBER ];     //数组，用于存储 epoll_wait 系统调用返回的事件
    int epollfd = epoll_create( 5 );        //传入 5 是为了表示最多监视 6 个文件描述符，但实际上这个参数对 epoll 的性能没有直接影响，可以传入任意大于 0 的值


/**************************************初始化定时器**************************************/
    //创建client_data结构体的数组*users_timer
    client_data *users_timer = new client_data[MAX_FD];

    Utils utils;

    Timer_Conn server(users, users_timer, &utils, TIMESLOT);

    

    utils.init(TIMESLOT);
    utils.addfd(epollfd, listenfd, false, 1);

    // fd添加到epoll对象中
    addfd( epollfd, listenfd, false );
    http_conn::m_epollfd = epollfd;

    int m_pipefd[2];
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(epollfd, m_pipefd[0], false, 0);

    // 对SIGPIE信号进行处理
    //当程序向一个 已经关闭的 TCP 连接（broken pipe） 写入数据时（例如客户端已断开但服务器仍在发送数据），
    //操作系统会默认触发 SIGPIPE 信号，导致进程意外终止。
    //SIG_IGN 表示显式忽略该信号
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    alarm(TIMESLOT);

    //工具类,信号和描述符基础操作
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = epollfd;



    bool timeout = false;

/*****************************************事件循环**************************************/
    while(true) {
        //epoll_wait拉取事件发生数
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) ) {   //如果返回值为负且不是中断错误，说明epoll失败，退出循环。
            printf( "epoll failure\n" );
            break;
        }
        //循环遍历事件数组：
        for ( int i = 0; i < number; i++ ) {
            
            int sockfd = events[i].data.fd; //新的连接请求到达：sockfd==listenfd；已经建立的连接上有数据可读或可写，sockfd==某个connfd
            
            if( sockfd == listenfd ) {
                // 有新客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof( client_address );

                while (1)   //while (1)就是边缘触发，不循环就是水平触发。（listenFD）
                {           // 不循环的时候，记得把下面两个if里面的break改成continue。
                
                            //accept 函数返回的socket，用于与已经成功连接到服务器的特定客户端进行数据交换。
                                //每次有新的客户端连接时，accept 会创建一个新的socket文件描述符，并将其与客户端连接相关联，listenfd仍用于监听。
                    int connfd = accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );   
                    
                    if ( connfd < 0 ) {
                        printf( "ET一次读完: %d\n", errno );  //
                        break;
                    } 

                    if( http_conn::m_user_count >= MAX_FD ) {
                        // 目前连接数满了
                        // 给客户端写一个信息： 服务器正忙
                        utils.show_error(connfd, "Internal server busy");
                        close(connfd);
                        break;
                    }
                    // 将新的客户的数据初始化， 放入数组中， 下次循环读取； 同时，将这个socket绑定到epollfd上。
                    // users[connfd].init( connfd, client_address);
                    server.user_and_timer_init( connfd, client_address);
                }
                
            
            } else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ) {
                // 对方异常断开或错误等事件
                util_timer *timer = users_timer[sockfd].timer;
                server.delete_socket_and_timer(timer, sockfd);
            
            } else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {

                int ret = 0;
                int sig;
                char signals[1024];
                ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1 || ret == 0)
                {
                    printf("deal with signal error\n");
                }
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        if(signals[i] == SIGALRM){
                            timeout = true;
                            break;
                        }
                    }
                }

            }
             else if(events[i].events & EPOLLIN) {

                util_timer *timer = users_timer[sockfd].timer;
                // 一次性把全部数据读完
                if(users[sockfd].read()) 
                {
                    pool->append(users + sockfd);   //users在之前初始化为数组的地址。
                    if (timer) server.adjust_timer(timer);
                } 
                else 
                {
                    server.delete_socket_and_timer(timer, sockfd);
                }

            }  else if( events[i].events & EPOLLOUT ) {

                util_timer *timer = users_timer[sockfd].timer;
                // 一次性把全部数据写完
                if( users[sockfd].write() ) 
                {
                    if (timer) server.adjust_timer(timer);
                }
                else {
                    users[sockfd].close_conn();
                }

            }
        }

        //定时器触发
        if(timeout){

            utils.timer_handler();
            timeout = false;
        }
    }
    
    close( epollfd );
    close( listenfd );
    delete [] users;
    delete pool;
    delete[] users_timer;
    return 0;
}













