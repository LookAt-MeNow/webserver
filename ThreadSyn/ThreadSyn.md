## 线程同步类 

**实现主要功能**：

- [x] 信号量
- [x] 互斥量
- [x] 条件变量

### 信号量基本操作

**sem_init**：初始化一个的信号量。

- 原型：int sem_init(sem_t *sem, int pshared, unsigned int value);
- 参数：
    - sem：指向信号量对象的指针。
    - pshared：如果非0，信号量在进程间共享；如果为0，信号量只能被当前进程的线程共享。
    - value：信号量的初始值。

**sem_destroy**：销毁信号量对象，释放其资源。
- 原型：int sem_destroy(sem_t *sem);
- 参数：
    - sem：指向信号量对象的指针。

**sem_wait**：等待信号量，减少信号量的值。如果信号量的值为0，则调用线程阻塞，直到信号量值大于0。
- 原型：int sem_wait(sem_t *sem);
- 参数：
    - sem：指向信号量对象的指针。

**sem_post**：增加信号量的值，如果有线程因等待该信号量而阻塞，它将被唤醒。
- 原型：int sem_post(sem_t *sem);
- 参数：
    - sem：指向信号量对象的指针。

### 互斥量基本操作
在多个线程中共享数据是，需要注意线程安全问题，如果多个线程同时访问同一个变量，并且其中至少一个线程进行了写操作，则出现数据竞争问题。

**pthread_mutex_init**：初始化一个互斥锁
- 原型: int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);
- 参数：
  - mutex：指向要初始化的互斥锁的指针。
  - attr：指向互斥锁属性的指针。如果传递NULL，则使用默认属性。

**pthread_mutex_destroy**:销毁一个互斥锁，释放其占用的资源。互斥锁在销毁之前必须是未锁定状态
- 原型：int pthread_mutex_destroy(pthread_mutex_t *mutex);
- 参数：
  - mutex：指向要销毁的互斥锁的指针

**pthread_mutex_lock**：以原子操作方式给互斥锁加锁。如果互斥锁已经被锁定，调用此函数的线程将阻塞，直到互斥锁变为可用
- 原型: int pthread_mutex_lock(pthread_mutex_t *mutex);
- 参数:
  - mutex：指向要加锁的互斥锁的指针

**pthread_mutex_unlock**:以原子操作方式给互斥锁解锁。解锁操作会使得一个等待该互斥锁的线程（如果有的话）变为可运行状态

- 原型: int pthread_mutex_unlock(pthread_mutex_t *mutex);
- 参数:
    - mutex：指向要解锁的互斥锁的指针

### 条件变量

**pthread_cond_init**：初始化一个条件变量

- 原型：int pthread_cond_init(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr);
- 参数：
  - cond：指向要初始化的条件变量的指针。
  - attr：指向条件变量属性的指针。如果传递NULL，则使用默认属性。

**pthread_cond_destroy**：销毁一个条件变量

- 原型: int pthread_cond_destroy(pthread_cond_t *cond);
- 参数: 
  - 指向要销毁的条件变量的指针

**pthread_cond_broadcast**: 以广播的方式唤醒所有等待目标条件变量的线程

- 原型: int pthread_cond_broadcast(pthread_cond_t *cond);
- 参数: 
  - cond：指向目标条件变量的指针 

**pthread_cond_wait**:函数用于等待目标条件变量.该函数调用时需要传入 mutex参数(加锁的互斥锁) ,函数执行时,先把调用线程放入条件变量的请求队列,然后将互斥锁mutex解锁,当函数成功返回为0时,互斥锁会再次被锁上. 也就是说函数内部会有一次解锁和加锁操作.

- 原型: int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex);
- 参数:
  - cond：指向等待的条件变量的指针。
  - mutex：指向已加锁的互斥锁的指针

----
**以上函数的返回值都是成功返回0，失败返回一个error**