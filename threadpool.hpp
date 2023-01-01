#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<vector>
#include<queue>
#include<memory>
#include<atomic>
#include<mutex>
#include<condition_variable>
#include<functional>

//Any类型：可以接受任意数据的类型
class Any
{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&)=delete;
    Any(Any &&)=default;
    Any& operator=(Any &&)=default;

    //Any类型接受其他任意类型的数据
    template<typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data))
    {}

    //将Any对象中存储的data数据提取出来
    template<tpyename T>
    T cast_()
    {
        
        Derive<T> *pd=dynamic_cast<Derive<T> *>(base_.get());//基类指针转派生类对象指针
        if(pd==nullptr)//基类与派生类不匹配
        {
            throw "type is numatch!";
        }
        return pd->data_;
    }

private:
    //基类类型
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    //派生类类型
    template<typeneme T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data)
        {}
    private:
        T data_;//任意的类型
    };

private:
    //定义一个基类的指针
    std::unique_ptr<Base> base_;
};
//任务抽象基类
class Task
{
public:
    //用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
    virtual Any run()=0;
private:    
};


//线程池支持的模式
enum class PoolMode  //枚举类
{
    MODE_FIXED, //固定数量的线程
    MODE_CACHED,    //线程数量可动态增长
};


//线程类型
class Thread
{
public:
    //线程函数对象类型
    using ThreadFunc=std::function<void()>;

    //线程构造
    Thread(ThreadFunc func);

    //线程析构
    ~Thread();
    
    //启动线程
    void start();

    //获取线程id
    int getId()const;

private:    
    ThreadFunc func_;
    //static int generatedId_;//??
    int threadId_; //保存线程id
};


/*
example:
ThreadPool pool;
pool.start(4);

class MyTask : public Task
{
public:
    void run(){ //线程任务代码... }        
}；
pool.submitTask(std::make_shard<MyTask>());

*/


//线程池类型
class ThreadPool
{
public:
    //线程池构造
    ThreadPool();

    //线程池析构
    ~ThreadPool();

    //设置线程池工作模式
    void setMode(PoolMode mode);

    //设置task任务队列上线阈值
    void setTaskQueMaxThreshHold(int threshhold);

    //给线程池提交任务
    void submitTask(std::shared_ptr<Task> sp);

    //开启线程池
    void start(int initThreadSize = 4 );

    ThreadPool(const ThreadPool&)=delete;
    ThreadPool& operator=(const ThreadPool&)=delete;

private:
    //定义线程函数
    void threadFunc();

    //检查pool的运行状态
    bool checkRunningState() const;

private:
    std::vector<std::unique_ptr<Thread>> threads_; //线程列表
    size_t initThreadSize_; //初始的线程数量

    std::queue<std::shared_ptr<Task>> taskQue_; //任务队列
    std::atomic_uint taskSize_; //任务数量
    int taskQueMaxThreshHold_;   //任务队列数量上线阈值

    std::mutex taskQueMtx_; //保证任务队列的线程安全
    std::condition_variable notFull_; //表示任务队列不满
    std::condition_variable notEmpty_; //表示任务队列不空

    PoolMode poolMode_; //当前线程池的工作模式


};



#endif