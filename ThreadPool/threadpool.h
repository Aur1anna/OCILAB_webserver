#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>

#include "../MemoryPool.h"
#include "../mysql_connect/mysql_conn.h"
#include "../locker.h"

// 线程池类，将它定义为模板类是为了代码复用，模板参数T是任务类
template<typename T>
class threadpool {
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number, int max_requests, connection_pool *connPool);
    ~threadpool();
    bool append(T* request);    //模板类T，保证可以append任何类型的任务

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void* worker(void* arg);
    void run();

private:
    // 线程的数量
    int m_thread_number;  
    
    // 描述线程池的数组，大小为m_thread_number    
    pthread_t * m_threads;

    // 请求队列中最多允许的、等待处理的请求的数量  
    int m_max_requests; 
    
    // 请求队列
    std::list< T* > m_workqueue;  

    // 保护请求队列的互斥锁
    locker m_queuelocker;   

    // 是否有任务需要处理
    sem m_queuestat;

    //数据库连接池
    connection_pool *m_connPool;  

    // 是否结束线程          
    bool m_stop;                    
};

template< typename T >
threadpool< T >::threadpool(int thread_number, int max_requests, connection_pool *connPool) :        
        m_thread_number(thread_number), 
        m_max_requests(max_requests), 
        m_connPool(connPool),
        m_stop(false), 
        m_threads(NULL) 
{

    if((thread_number <= 0) || (max_requests <= 0) ) {  // 线程数量和请求队列大小必须大于0，否则传出异常
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) {
        throw std::exception(); 
    }

    // 创建thread_number 个线程，并将他们设置为脱离线程。
            //脱离线程，则线程结束时会自动被回收资源，不用等待主线程的退出。
    for ( int i = 0; i < thread_number; ++i ) {
        printf( "create the %dth thread\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this ) != 0) {   //m_threads + i指的是地址，worker是线程执行的入口函数，this指的是对象
            delete [] m_threads;    //如果没有创建成功，则释放线程数组
            throw std::exception();
        }
        
        if( pthread_detach( m_threads[i] ) ) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}


template< typename T >
threadpool< T >::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool< T >::append( T* request )
{
    // 操作工作队列时一定要加锁，因为它被所有线程共享。
    m_queuelocker.lock();
    if ( m_workqueue.size() >= m_max_requests ) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template< typename T >
void* threadpool< T >::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run() {

    while (!m_stop) {
        m_queuestat.wait();
        m_queuelocker.lock();
        if ( m_workqueue.empty() ) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( !request ) {
            continue;
        }
        connectionRAII mysqlcon(&request->mysql, m_connPool);
        request->process();
    }

}

#endif
