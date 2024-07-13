#ifndef _THREADSYN_H_
#define _THREADSYN_H_

//封装线程同步类
//1.信号量，2.互斥量，3.条件变量

#include <thread>
#include <exception>
#include <semaphore.h>

//信号量
class sem{
    public:
        sem(){
            if(sem_init(&m_sem,0,0) != 0) { //1.初始化信号量m_sem 2.信号量在线程间共享，3. 初始值为0
                throw std::exception(); //在构造函数中初始化信号量，若失败则抛出异常
            } 
        }
        sem(int num){
            if(sem_init(&m_sem,0,num) != 0) { //1.初始化信号量m_sem 2.信号量在线程间共享，3. 初始值为num
                throw std::exception(); //在构造函数中初始化信号量，若失败则抛出异常
            }
        }
        //等待信号量 P操作
        bool wait(){
            if(sem_wait(&m_sem) != 0) { //等待信号量，若失败则返回false
                return false;
            }
            return true;
        }
        //增加信号量 V操作
        bool signal(){
            if(sem_post(&m_sem) != 0) { //增加信号量，若失败则返回false
                return false;
            }
            return true;
        }
        ~sem(){
            sem_destroy(&m_sem); //销毁信号量
        }

    private:
        sem_t m_sem;
};

//互斥量
class locker{
    public:
        locker(){
            if(pthread_mutex_init(&m_mutex,NULL) != 0) {
                throw std::exception();
            }
        }
        //加锁
        bool lock(){
            if(pthread_mutex_lock(&m_mutex) != 0) {
                return false;
            }
            return true;
        }
        //解锁
        bool unlock(){
            if(pthread_mutex_unlock(&m_mutex) != 0) {
                return false;
            }
            return true;
        }
        ~locker(){
            pthread_mutex_destroy(&m_mutex);
        }

    private:
        pthread_mutex_t m_mutex;
};

//条件变量

class cond{
    cond(){
        if(pthread_mutex_init(&m_mutex,NULL) != 0) {
            throw std::exception();
        }
        if(pthread_cond_init(&m_cond,NULL) != 0) {
            pthread_mutex_destroy(&m_mutex);
            throw std::exception();
        }
    }
    //等待条件变量
    bool wait(){
        int ret = 0;
        pthread_mutex_lock(&m_mutex);
        ret = pthread_cond_wait(&m_cond,&m_mutex);
        pthread_mutex_unlock(&m_mutex);
        return (ret == 0) ? true : false;
    }
    //唤醒等待条件变量的线程
    bool signal(){
        return (pthread_cond_signal(&m_cond) == 0) ? true : false;
    }
    //唤醒所有等待条件变量的线程
    bool broadcast(){
        return (pthread_cond_broadcast(&m_cond) == 0) ? true : false;
    }
    ~cond(){
        pthread_mutex_destroy(&m_mutex);
        pthread_cond_destroy(&m_cond);
    }

    private:
        pthread_mutex_t m_mutex; //条件变量需要与互斥量配合使用
        pthread_cond_t m_cond;
}; 

#endif