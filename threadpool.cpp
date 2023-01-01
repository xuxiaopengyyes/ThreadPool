#include"threadpool.hpp"

#include<functional>
#include<iostream>
#include<thread>
#include<memory>
#include<chrono>
const int TASK_MAX_THRESHHOLD=1024;


//线程池构造
 ThreadPool::ThreadPool()
    :initThreadSize_(0)
    ,taskSize_(0)
    ,taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
    ,poolMode_(PoolMode::MODE_FIXED)
 {}

 //线程池析构
ThreadPool::~ThreadPool()
{}

//设置线程池工作模式
void ThreadPool::setMode(PoolMode mode)
{
    poolMode_=mode;
}


//设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
    taskQueMaxThreshHold_=threshhold;
}

//给线程池提交任务  用户调用该接口  传入任务对象，生产任务
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    //获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    //线程的通信  等待任务队列有空余
    //用户提交任务，最长不能阻塞超过1s，否则判断提交任务失败，返回  //任务降级
    /*
        //等价
        while(taskQue_size()==taskQueMaxThreshHold_)
        {
            notFull_.wait(lock);
        }
        notFull_.wait(lock,[]()->bool{return taskQue_size() < taskQueMAxThresh_;});
    */
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),
        []()->bool{return taskQue_size() < taskQueMAxThresh_;}))
    {
        //表示notFull_等待超过1s,条件依然没有满足
        std::cerr<< "task queue is full,submit task fail." <<std::endl;
    }

    //如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    //因为新放了任务，任务队列肯定不空，notEmpty_上进行通知,赶快分配线程执行任务
    notEmpty_notify_all();
}

//开启线程池
void ThreadPool::start(int initThreadSize )
{
    //记录初始线程个数
    initThreadSize_=initThreadSize;

    //创建线程对象
    for(int i=0;i<initThreadSize_;i++)
    {
        //创建thread线程对象的时候，把线程函数给到thread线程对象
        auto ptr=std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this));
        threads_.emplace_back(std::move(ptr));//原位构造
    }

    //启动所有线程 std::vector<Thread*> threads_;
    for(int i=0;i<initThreadSize_;i++)
    {
        threads_[i]->start();//需要去执行一个线程函数
    }
}

//定义线程函数
void ThreadPool::threadFunc()
{
    std::cout<<"begin thread func   "<<std::this_thread::get_id()<<std::endl;
    std::cout<<"end thread func   "<<std::this_thread::get_id()<<std::endl;
}

///////////////////线程方法实现

//线程构造
Thread::Thread(ThreadFunc func)
    :func_(func)
{}

//线程析构
Thread::~Thread()
{

}

//启动线程
void Thread::start()
{
    std::thread t(func_);
    t.detach(); //设置分离线程
}