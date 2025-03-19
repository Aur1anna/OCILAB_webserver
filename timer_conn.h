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
#include "locker.h"
#include "./ThreadPool/threadpool.h"
#include "./mysql_connect/mysql_conn.h"
#include "./http/http_conn.h"





class Timer_Conn {

public:

    Timer_Conn(http_conn* users, client_data* users_timer, Utils* utils, int timeslot) {
        this->users = users;
        this->users_timer = users_timer;
        this->utils = utils;
        TIMESLOT = timeslot;
    }
    //~Timer_Conn();
    void user_and_timer_init(int connfd, struct sockaddr_in client_address)
    {
        //将新的客户的数据初始化， 放入数组中， 下次循环读取； 同时，将这个socket绑定到epollfd上。
        users[connfd].init(connfd, client_address);

        //初始化client_data数据
        //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
        users_timer[connfd].address = client_address;
        users_timer[connfd].sockfd = connfd;
        util_timer *timer = new util_timer;
        timer->user_data = &users_timer[connfd];
        timer->cb_func = cb_func;
        time_t cur = time(NULL);
        timer->expire = cur + 3 * TIMESLOT;
        users_timer[connfd].timer = timer;
        utils->m_timer_lst.add_timer(timer);
    }

    //若有数据传输，则将定时器往后延迟3个单位
    //并对新的定时器在链表上的位置进行调整
    void adjust_timer(util_timer *timer)
    {
        time_t cur = time(NULL);
        timer->expire = cur + 3 * TIMESLOT;
        utils->m_timer_lst.adjust_timer(timer);
    }

    void delete_socket_and_timer(util_timer *timer, int sockfd)
    {
        timer->cb_func(&users_timer[sockfd]);
        if (timer)
        {
            utils->m_timer_lst.del_timer(timer);
        }
    }

    bool deal_with_signal(bool &timeout, int m_pipefd[])
    {
        int ret = 0;
        int sig;
        char signals[1024];
        ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
        if (ret == -1 || ret == 0)
        {
            return false;
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
        return true;
    }


private:

    http_conn* users;
    client_data* users_timer;
    Utils* utils;

    int TIMESLOT;
};