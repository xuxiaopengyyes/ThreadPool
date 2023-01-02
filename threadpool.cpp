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
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
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
        notFull_.wait(lock,[]()->bool{return taskQue.size() < taskQueMAxThresh_;});
    */
    if(!notFull_.wait_for(lock,std::chrono::seconds(1),
        [&]()->bool{return taskQue_.size() < (size_t)taskQueMaxThreshHold_;}))
    {
        //表示notFull_等待超过1s,条件依然没有满足
        std::cerr<< "task queue is full,submit task fail." <<std::endl;
        return ;
    }

    //如果有空余，把任务放入任务队列中
    taskQue_.emplace(sp);
    taskSize_++;

    //因为新放了任务，任务队列肯定不空，notEmpty_上进行通知,赶快分配线程执行任务
    notEmpty_.notify_all();

    

    //返回任务的Result对象
    return Result(sp);

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
    /*
    std::cout<<"begin thread func   "<<std::this_thread::get_id()<<std::endl;
    std::cout<<"end thread func   "<<std::this_thread::get_id()<<std::endl;
    */

   for(;;)
   {
        std::shared_ptr<Task> task;
        {
            //先获取锁
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            //等待notEmpty条件
            notEmpty_.wait(lock,[&]()->bool{return taskQue_.size() > 0;});

            //从任务队列中取一个任务出来   
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            //如果依然有剩余任务，继续通知其他的线程执行任务
            if(taskQue_.size() > 0)
            {
                notEmpty_.notify_all();
            }
            
            //取出一个任务，进行通知,可以继续提交生产任务
            notFull_.notify_all();

        } //取完任务后应该把锁释放掉

        //当前线程负责执行这个任务
        if(task!=nullptr)
        {
            task->exec();
        }
   }
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

//////////////////////   Task方法的实现
Task::Task()
    :result_(nullptr)
{}

void Task::exec()
{
    if(result!=nullptr)
    {
        result_->setaVal(run());// 发生多态调用
    }
}

void Task::setResult(Result *res)
{
    result_=res;
}


//////////////////////   Result方法的实现
Result::Result(std::shard_ptr<Task> task,bool isvalid)
    :isvalid_(isvalid)
    ,task_(task)
{
    task_->setResult(this);
}

Any Result::get()
{
    if(!isValid_)
    {
        return "";
    }
    sem_.wait();
    return std::move(any_);
}

void Result::setaVal(Any any)
{
    //存储task的返回值
    this->any_=std::move(any);
    sem_.post();// 已经获取的任务的返回值，增加信号量资源
}