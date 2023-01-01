#include<iostream>
#include<thread>
#include<chrono>
#include "threadpool.hpp"
int main()
{
    ThreadPool pool;
    pool.start(4);
    std::this_thread::sleep_for(std::chrono::seconds(5));

    
    return 0;
}