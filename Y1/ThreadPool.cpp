
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
