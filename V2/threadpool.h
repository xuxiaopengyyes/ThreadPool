#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<iostream>
#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>
#include<unordered_map>
#include<thread>
#include<future>

const int TASK_MAX_THRESHHOLD = 2; //INT32_MAX
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60; //s //超时时间，回收线程

//线程池支持的模式
enum class PoolMode //枚举类
{
    MODE_FIXED, //固定数量的线程
    MODE_CACHED, //线程数量可动态增长
};

//线程类型
class Thread
{
public:
    //线程函数对象类型
    using ThreadFunc = std::function<void(int)>;

    //线程构造
    Thread(ThreadFunc func)
        :func_(func)
        ,threadId_(generateId_++)
    {}

    //线程析构
    ~Thread() = default;

    //启动线程
    void start()
    {
        //创建一个线程来执行一个线程函数 pthread_creat
        std::thread t(func_,threadId_);
        t.deatch(); //分离线程 pthread_detach
    }

    //获取线程id
    int getId()
    {
        return threadId_;
    }

private:
    ThreadFunc func_;
    static int generateId_; //自增键，用于线程id
    int threadId_; //保存线程id
};

int Thread::generateId_ = 0;

//线程池类型
class ThreadPool
{
public:
    //线程池构造
    ThreadPool()
        : initThreadSize_(0)
        , taskSize_(0)
        , idleThreadSize_(0)
        , curThreadSize_(0)
        , taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
        , threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
        , poolMode_(PoolMode::MODE_FIXED)
        , isPoolRunning_(false)
    {}

    //线程池析构
    ~ThreadPool()
    {
        isPoolRunning_ = false;

        //等待线程池里面所有的线程返回 有两种状态： 阻塞& 正在执行任务
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        notEmpty_.notify_all();
        exitCond_.wait(Lock, [&]()->bool {return threads_.size() == 0; });
    }

    //设置线程池的工作模式
    void setMode(PoolMode mode)
    {
        if(checkRunningState())
            return;
        poolMode_ = mode;
    }

    //设置task任务队列的上限阈值
    void setTaskQueMaxThreshHold(int threshhold)
    {
        if(checkRunningState())
            return;
        taskQueMAxThreshHold_ = threshhold;
    }

    //设置线程cached模式下线程阈值
    void setThreadSizeThreshHold(int threshHold)
    {
        if(checkRunningState())
            return;
        if(poolMode_ == PoolMode::MODE_CACHED)
        {
            threadSizeThreshHold_ threshHold;
        }
    }

    //给线程提交任务

    template<typename Func, typename... Args>
    auto submitTask(Func&& func, Args&&... args) -> std::future<decltype(func(args...))>
    {
        // 打包任务，放入任务队列
        using RType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func),std::forward<Args>(args)... ));
        std::future<Rtype> result = task->get_future();

        //获取锁
        std::unique_lock<std::mutex> lock(taskQueMtx_);
        //用户提交任务，最长不能超过1s,否则做任务降级处理（提交任务失败）
        if(!notFull_.wait_for(lock,std::chrono::seconds(1),
            [&]()->bool { return taskQue_.size() < (size_t)taskQueMaxThreshHold_;}))
        {
            //notFull_等待1s后，条件还没有满足
            std::cerr << "task queue is full, submit task fail. "<<std::endl;
            auto task = std::make_shared<std::packaged_task<RType()>>(
                []()->RType {return RType(); });
            (*task)();
            return task->get_future();
        }

        //如果有空余，把任务放入任务队列中
        taskQue_.emplace([task](){*(task)();});
        taskSize_++;

        //因为新放了任务，任务队列肯定不空了，在notEmpty_上进行通知，赶快分配线程执行任务
        notEmpty_notify_all();

        //cached模式 任务模式比较紧急 场景 ： 小而快的任务，需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程出来
        if(poolMode_ == PoolMode::MODE_CACHED
            && taskSize_ > idleThreadSize_
            && curThreadSize_ < threadSizeThreshHold_)
        {
            std::cout << ">>> create new thread... "<<std::endl;

            //创建新的线程对象
            auto ptr = std::make_unique<Thread>(std::bind(&threadPool::threadFunc,this,std::placeholders::_1));
            int threadId = ptr->getId();
            threads_emplace(threadId, std::move(ptr));
            //启动线程
            threads_[threadId]->start();
            // 修改线程个数相关的变量
            curThreadSize_++;
            idleThreadSize_++;
        }

        //返回任务的Result对象
        return result;
    }

    	// 开启线程池
	void start(int initThreadSize = std::thread::hardware_concurrency())
	{
		// 设置线程池的运行状态
		isPoolRunning_ = true;

		// 记录初始线程个数
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;

		// 创建线程对象
		for (int i = 0; i < initThreadSize_; i++)
		{
			// 创建thread线程对象的时候，把线程函数给到thread线程对象
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			// threads_.emplace_back(std::move(ptr));
		}

		// 启动所有线程  std::vector<Thread*> threads_;
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start(); // 需要去执行一个线程函数
			idleThreadSize_++;    // 记录初始空闲线程的数量
		}
	}

    ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
    //检查pool的运行状态
    bool checkRunningState() const 
    {
        return isPoolRunning_;
    }
private:
    std::unordered_map<int,std::unique_ptr<Thread>> threads_; //线程列表

    int initThreadSize; //初始的线程数量
    int threadSizeThreshHold_; //线程数量上限阈值
    std::atomic_int curThreadSize_; //记录当前线程池里面线程的总数量
    std::atomic_int idleThreadSize_; //记录空闲线程的数量

    //Task任务  -》 函数对象 
    using Task = std::function<void()>;
    std::queue<Task> taskQue_; //任务队列
    std::atomic_int taskSize_(); //任务的数量
    int taskQueMaxThreshHold_; //任务队列数量上限阈值 

    std::mutex taskQueMax_; //保证任务队列的线程安全
    std::condition_variable notFull_;   //表示任务队列不满
    std::condition_variable notEmpty_;  //表示任务队列不空
    std::condition_variable exitCond_; //等待线程资源全部回收

    PoolMode poolMode_; //表示线程池的工作模式
    std::atomic_bool isPoolRunning_; //表示当前线程池的启动状态
};

#endif