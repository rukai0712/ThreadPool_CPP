#pragma once

#include<iostream>
#include<mutex>
#include<cassert>
#include<condition_variable>
#include<chrono>

class ThreadSemaphore
{
    public:
        ThreadSemaphore(int value = 0)
            :m_count(value)
        {
            assert(m_count >= 0);
        }

        ~ThreadSemaphore()
        {}

        void Wait();

        void Post();

        bool TryWait();

        template< typename Rep, class Period>
            bool TimedWait(const std::chrono::duration<Rep, Period>& wait_time);

        bool TimedWait(int milliseconds_time)
        {
            return TimedWait(std::chrono::milliseconds(milliseconds_time));
        }

        int GetValue()
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            return m_count;
        }

    private:
        std::mutex m_mutex;
        std::condition_variable m_weakup;
        int m_count;
};



void ThreadSemaphore::Wait()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_count == 0)
    {
        m_weakup.wait(lk, [this]{return m_count > 0;});
    }
    --m_count;
}

void ThreadSemaphore::Post()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    ++m_count;
    m_weakup.notify_one();
}

bool ThreadSemaphore::TryWait()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_count == 0)
        return false;
    --m_count;
    return true;
}

template< typename Rep, class Period>
bool ThreadSemaphore::TimedWait(const std::chrono::duration<Rep, Period>& wait_time)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_count == 0)
    {
        if (!m_weakup.wait_for(lk, wait_time,[this] {return m_count > 0;}))
            return false;
    }
    --m_count;
    return true;
}



