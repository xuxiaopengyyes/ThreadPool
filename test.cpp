#include<iostream>
#include<thread>
#include<chrono>
using namespace std;

#include "threadpool.hpp"


/*
有些场景，希望获得线程执行任务的返回值
example:
1+ ... +30000的和
thread1  1+...+10000
thread2  10001+...+20000
...

main thread: 给每一个线程分配计算的区间，并等待他们算完返回结果，合并最终的结果
*/

class MyTask : public Task
{
public:
    MyTask(int begin,int end)
        :begin_(begin)
        ,end_(end)
    {}
    //问题一：如何设计run函数的返回值，可以表示任意的类型
    //java python Object 是所有其他类类型的基类
    //C++17 Any类型

    void run()
    {
        std::cout<< "tid: "<<std::this_thread::get_id()<<"begin! "<<std::endl;
        //std::this_thread::sleep_for(std::chrono::seconds(2));
        int sum=0;
        for(int i=begin_;i<=end_;i++)
        {
            sum+=i;
        }
        std::cout<< "tid: "<<std::this_thread::get_id() <<"end! "<<std::endl;
        return sum;
    }
private:
    int begin_;
    int end_;
};


int main()
{
    ThreadPool pool;
    pool.start(4);
    
    //如何设计Result机制
    Result res = pool.submitTask(std::make_shared<MyTask>());
    
    int sum=res.get().cast_<int >();
    
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());
    pool.submitTask(std::make_shared<MyTask>());


    getchar();

    
    return 0;
}