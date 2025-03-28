#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>        //包含互斥锁
#include <semaphore.h>

// 线程同步机制封装类

// 互斥锁类
class locker {
public:
    locker() {
        if(pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }

    ~locker() {
        pthread_mutex_destroy(&m_mutex);
    }

    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};


// 条件变量类
class cond {
public:
    cond(){
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }
    ~cond() {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex) {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};


// 信号量类
class sem {
public:
    sem() {
        if( sem_init( &m_sem, 0, 0 ) != 0 ) {   //第二个参数 0 表示信号量在线程间共享（非进程间），第三个参数表示初始值
            throw std::exception();
        }
    }
    sem(int num) {
        if( sem_init( &m_sem, 0, num ) != 0 ) {
            throw std::exception();
        }
    }
    ~sem() {
        sem_destroy( &m_sem );
    }


    // 限制同时访问某资源的线程数量（如线程池任务队列）：
    bool wait() {
        return sem_wait( &m_sem ) == 0;     //：减少信号量（P操作），若值为 0 则阻塞。
    }
    bool post() {
        return sem_post( &m_sem ) == 0;     //：增加信号量（V操作），唤醒等待线程。
    }
private:
    sem_t m_sem;
};

#endif