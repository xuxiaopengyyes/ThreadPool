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
   
    //java python Object 是所有其他类类型的基类
    //C++17 Any类型

    Any run()
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

using UL=unsigned long long;

int main()
{
    ThreadPool pool;
    pool.start(4);
    
    //如何设计Result机制
    Result res1 = pool.submitTask(std::make_shared<MyTask>(1,100000000));
    Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
    Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
    //pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));


    UL sum1 = res1.get().cast_<UL>();  
    UL sum2 = res2.get().cast_<UL>();
    UL sum3 = res3.get().cast_<UL>();

    cout << (sum1 + sum2 + sum3) << endl;
    getchar();

    
    return 0;
}