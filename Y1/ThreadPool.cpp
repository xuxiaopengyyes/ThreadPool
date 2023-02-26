
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
        if(!m_notFull.wait_for(locker,std::chrono::seconds(1),
            [this] {return m_needStop || !IsFull();}))
        {
            std::cout<< "task queue full ..." <<std::endl;
            return 1;
        }
        if(m_needStop)
        {
            std::cout<< "queue stop ..."<<std::endl;
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

    template<typename F>
    int Put(F&& x)
    {
        return Add(std::forword< F >(x));
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
            std::cout<< "queue stop ..."<<std::endl;
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
        if(m_needStop)
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
        std::unique_lock<std::mutex> locker(m_mutex);
        return m_queue.size();
    }
};

const int MaxTaskCount = 2;
class CachedThreadPool
{
public:
    using Task = std::function<void(void)>;
private:
    std::unordered_map<std::thread::id, std::shared_ptr<std::thread> > m_threadgroup;
    int m_coreThreadSize;
    int m_maxThreadSize;
    std::atomic_int m_idleThreadSize;
    std::atomic_int m_curThreadSize;
    mutable std::mutex m_mutex;
    SyncQueue<Task> m_queue;
    std::atomic_bool m_running;
    std::once_flag m_flag;
    void Start(int numthreads)
    {
        m_running = true;
        m_curThreadSize = numthreads;
        for(int i=0; i < numthreads; ++i)
        {
            auto tha = std::make_shared<std::thread>(std::thread(&CachedThreadPool::RunInThread,this));
            std::thread::id tid = tha->get_id();
            //是否分离线程？？？？
            m_threadgroup.emplace(tid, std::move(tha));
            m_idleThreadSize++;
        }
    }
    void RunInThread()
    {
        auto tid = std::this_thread::get_id();
        auto startTime = std::chrono::high_resolution_clock().now();
        while(m_running)
        {
            Task task;
            if(m_queue.Size() == 0 && m_queue.notTask())
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto now = std::chrono::high_resolution_clock().now();
                auto intervalTime = std::chrono::duration_cast<std::chrono::seconds>(now-startTime);
                if(intervalTime.count() >= KeepAliveTime && m_curThreadSize > m_coreThreadSize)
                {
                    m_threadgroup.find(tid)->second->detach();//???
                    m_threadgroup.erase(tid);
                    m_curThreadSize--;
                    m_idleThreadSize--;
                    std::cout << "free thread idle" <<m_curThreadSize <<" "<<m_coreThreadSize <<std::endl;
                    return ;
                }
            }
            if(!m_queue.Take(task) && m_running)
            {
                m_idleThreadSize--;
                task();
                m_idleThreadSize++;
                startTime = std::chrono::high_resolution_clock().now();
            }
        }
    }
    void StopThreadGroup()
    {
        m_queue.Stop();
        m_running =false;

        for(auto& thread : m_threadgroup)
        {
            thread.second->join();
        }
        m_threadgroup.clear();
    }
public:
    CachedThreadPool(int initNumThreads,int taskPoolSize = MaxTaskCount)
        :m_coreThreadSize(initNumThreads)
        ,m_maxThreadSize(2 * std::thread::hardware_concurrency() + 1)
        ,m_idleThreadSize(0)
        ,m_curThreadSize(0)
        ,m_queue(taskPoolSize)
        ,m_running(false)
    {
        Start(m_coreThreadSize);
    }
    ~CachedThreadPool()
    {
        Stop();
    }
    void Stop()
    {
        std::call_once(m_flag, [this] {StopThreadGroup();});
    }

    template<class Func, class... Args>//decltype是如何推导的
    auto AddTask(Func&& func,Args&&... args)->std::future<decltype(func(args...))>
    {
        using RetType = decltype(func(args...));
        auto task = std::make_shared<std::packaged_task<RetType()> >(
            std::bind(std::forword<Func>(func),std::forword<Args>(args)...));
        std::future<RetType> result = task->get_future();

        if(m_queue.Put([task]() {(*task)();}) != 0 )
        {
            std::cout<< "who run 调用者运行策略" <<std::endl;
            (*task)();
        }
        if(m_idleThreadSize <= 0 && m_curThreadSize < m_maxThreadSize)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto tha = std::make_shared<std::thread> (std::thread(&CachedThreadPool::RunInThread,this));
            std::thread::id tid = tha->get_id();

            m_threadgroup.emplace(tid, std::move(tha));
            m_idleThreadSize++;
            m_curThreadSize++;
        }
        return result;
    }
};
CachedThreadPool pool(2);

int add(int a,int b,int s)
{
    std::this_thread::sleep_for(std::chrono::seconds(s));
    int c=a+b;
    std::cout<< "add begin ..." <<std::endl;
    return c;
}

int add_a()
{
    auto r=pool.AddTask(add,10,20,4);
    std::cout<<"add_a: "<<r.get() <<std::endl;
}

int add_b()
{
    auto r=pool.AddTask(add,20,30,6);
    std::cout<<"add_b: "<<r.get() <<std::endl;
}

int add_c()
{
    auto r=pool.AddTask(add,30,40,1);
    std::cout<<"add_c: "<<r.get() <<std::endl;
}

int add_d()
{
    auto r=pool.AddTask(add,10,40,9);
    std::cout<<"add_d: "<<r.get() <<std::endl;
}

int main()
{
	std::thread tha(add_a);
	std::thread thb(add_b);
	std::thread thc(add_c);
	std::thread thd(add_d);

	tha.join();
	thb.join();
	thc.join();
	thd.join();

	std::this_thread::sleep_for(std::chrono::seconds(20));
	std::thread the(add_a);
	std::thread thf(add_b);
	the.join();
	thf.join();

	return 0;
}