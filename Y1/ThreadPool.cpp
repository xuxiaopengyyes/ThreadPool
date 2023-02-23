
#include<list>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<iostream>
#include<memory>
#include<atomic>
#include<functional>
#include<future>
#include<unordered_map>

const int KeepAliveTime = 10;
template<class T>
class SyncQueue
{
private:
	std::list<T> m_queue; // 任务缓冲区
	mutable std::mutex m_mutex;
	std::condition_variable m_notEmpty;	//  C
	std::condition_variable m_notFull;  //	P
	int m_maxSize;          // 
	bool m_needStop;    // true 同步队列不在接受新的任务// 

	bool IsFull() const
	{
		bool full = m_queue.size() >= m_maxSize;
		if (full)
		{
			printf("m_queue full....\n");
		}
		return full;
	}
	bool IsEmpty() const
	{
		bool empty = m_queue.empty(); // 
		if (empty)
		{
			printf("m_queue empty....\n");
		}
		return empty;
	}

    //return 0;//成功
    //       1;//full
    //       2;//stop
    template<class F>
    int Add(F&& x)
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        if(!M_notFull.wait_for(lock,std::chrono::seconds(1),
            [this] {return m_needStop || !IsFull();}))
        {
            cout<< "task queue full ..." <<endl;
            return 1;
        }
        if(m_needStop)
        {
            cout<< "queue stop ..."<<endl;
            return 2;
        }

        m_queue.push_back(std::forword<F>(x));
        m_notEmpty.notify_one();
        return 0;
    }

public:
    SyncQueue(int maxsize = 1024)
        :m_maxSize(maxsize)
        ,m_needStop(false) // 同步队列开始工作
    {}

    template<class F>
    int Put(F&& x)
    {
        return Add(std::forword<F>(x));
    }

    int notTask()
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        if(!m_needStop && 
            m_notEmpty.wait_for( locker, std::chrono::seconds(1)) == std::cv_status::timeout)
        {
            return 1;
        }
        return 0;
    }

    void Take(std::list<T>& list) //取整个任务队列
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        while (!m_needStop && IsEmpty() )
        {
            m_notEmpty.wait(locker);
        }
        if(m_needStop)
        {
            cout<< "queue stop ..."<<endl;
        }

        list = std::move(m_queue);
        m_notFull.notify_one();
    }

    //return 0; //成功
    //       1; //empty
    //       2; //stop
    /*
        注意当Take 因超时或停止 而错误返回时，函数对象t为空
    */
    int Take(T& t)
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        if(!m_notEmpty.wait_for(locker, 
            std::chrono::seconds(1), [this] {return m_needStop || !IsEmpty();}))
        {
            return 1;
        }
        if(m_needStop())
        {
            return 2;
        }
        t = m_queue.front();
        m_queue.pop_front();
        m_notFull.notify_one();
        return 0;
    }

    void Stop()
    {
        {
            std::unique_lock<std::mutex> locker(m_mutex);
            m_needStop = true;
        }//及时放开锁，否则其他线程无法从条件变量退出
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    bool Empty() const
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.empty();
    }

    bool Full() const
    {
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.size() >=m_maxSize;
    }

    size_t Size() const
    {
        std::unique_lock<std::mutex> locker(m_muntex)
        return m_queue.size();
    }