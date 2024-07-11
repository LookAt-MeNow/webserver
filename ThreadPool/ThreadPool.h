#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#include <queue>
#include <thread>
#include <exception>
#include "../ThreadSyn/ThreadSyn.h"

//线程池类
template<typename T>
class ThreadPool{
    pubile:
        ThreadPool();
        ThreadPool(int max_request,int num_thread);
        ~ThreadPool();
        bool append_task(T* task); //添加任务
    private: //线程处理函数和线程运行函数设置为私有
        static void* worker(void* arg); //使用静态函数作为工作函数,pthread_create不接受成员函数，需要设置为static
        void run(); //线程运行函数
    private:
        std::queue<T*> task_queue; //任务队列
        std::vector<std::thread_t> arr_threads; //线程池数组，大小为num_thread
        int max_request; //线程池最大请求数
        int num_thread; //线程池线程数
        locker m_locker; //保护请求队列的互斥锁
        sem m_sem; //用于判断是否有任务需要处理
        bool cease; //线程结束标志
};

template<typename T>
ThreadPool<T>::ThreadPool(int qmax_request,int qnum_thread):max_request(qmax_request),num_thread(qnum_thread),cease(false),arr_thread(NULL){
    if(max_request <= 0 || num_thread <= 0) {
        throw::std::exception(); //线程池大小和线程数必须大于0
    }
    //线程初始化
    if(!(arr_threads = new std::thread_t[num_thread])){
        throw::std::exception(); //如果new失败，返回空指针,(空被认为是false)抛出异常
    }
    //创建qnum_thread个线程
    for(int i = 0;i < qnum_thread;i++) {
        if(pthread_create(arr_threads + i,NULL,worker,this) != 0) { //arg1:线程id arg2:线程属性 arg3:线程函数 arg4:线程函数参数
            delete[] arr_threads; // 创建失败 释放内存
            throw::std::exception();
        }
        //线程分离
        if(pthread_detach(arr_threads[i]) != 0) {
            throw::std::exception(); 
        }
    }

}

//析构函数
template<typename T>
ThreadPool<T>::~ThreadPool(){
    delete[] arr_threads; //释放线程池数组
    cease = true; //线程结束标志
}

//添加任务
template<typename T>
bool ThreadPool<T>::append_task(T* task) {
    m_locker.lock(); //加锁
    if(task_queue.size() > max_request) { //队列数大于最大线程数
        m_locker.unlock(); //解锁
        return false;
    }
    task_queue.push(task); //添加任务
    m_locker.unlock();
    m_sem.post(); //信号量加1
    return true;
}

#endif