#pragma once

#include<iostream>
#include<thread>
#include<future>
#include<mutex>
#include<condition_variable>
#include<chrono>
#include<atomic>
#include<cassert>

#define DEBUG

#ifdef DEBUG
std::mutex print_mutex;
#endif

/*
 *
 */

class IThreadTask
{
    public:
        virtual ~IThreadTask()
        {}
        virtual void Execute() = 0;
};

/*
 * 用以实现将可调用对象打包成IThreadTask的派生类
 */
template<typename result_type>
class SimpleThreadTask: public IThreadTask
{
    public:
        template <class Function, class... Args>
        explicit SimpleThreadTask(Function&& f, Args&&... args)
        :m_task (std::bind(std::forward<Function>(f), std::forward<Args>(args)...))
        {}

        virtual ~SimpleThreadTask() override
        {}

        virtual void Execute() override
        {
            m_task();
        }

        std::future<result_type> GetFuture()
        {
            return m_task.get_future();
        }
    private:
        std::packaged_task<result_type()>   m_task;
};

/*
 * SimpleThreadTask 类的工厂函数
 * 返回future对象和unique_ptr<IThreadTask>对象
 */
template< typename Function, typename... Args>
auto  CreateSimpleThreadTask(Function&& f, Args&&... args ) -> std::pair<std::future<typename std::result_of<Function(Args...)>::type>, std::unique_ptr<IThreadTask>> 
{
    typedef typename std::result_of<Function(Args...)>::type    result_type;

    std::unique_ptr<SimpleThreadTask<result_type>>              task(new SimpleThreadTask<result_type> (std::forward<Function>(f), std::forward<Args>(args)...));
    std::future<result_type>                                    res(task->GetFuture());
    return std::make_pair(std::move(res), std::move(task));
}

/*
 *
 */
class ThreadPool
{
    public:
        template< class Rep, class Period >
            ThreadPool
            (
             int min_thread_num,
             int max_thread_num,
             int default_thread_vary,
             int queue_max_size,
             int waiting_tasks_danger_mark,
             std::chrono::duration<Rep, Period> const& adjust_interval_time
            );

        ~ThreadPool();
        void StartPool();
        void ShutdownPool();

        void SubmitTask(std::unique_ptr<IThreadTask> new_task);

    private:
        void StartThread();
        void AdjustPool();
    private:
        int const MIN_THREAD_NUM;                       //线程池中最小线程数
        int const MAX_THREAD_NUM;                       //线程池中最大线程数
        int const DEFAULT_THREAD_VARY;                  //默认线程增减的数量
        int const MAX_QUEUE_SIZE;                       //任务队列最大值
        int const WAITING_TASKS_DANGER_MARK;            //队列中允许未处理任务的数量，超过表示需要增加线程个数
        std::chrono::nanoseconds const ADJUST_INTERVAL_TIME;    //AdjustPool调整线程池的时间间隔

        bool m_shutdown;                                //记入线程池是否出于关闭状态
        int m_queue_front;                              //记入队头编号
        int m_queue_rear;                               //记入队尾编号
        std::atomic<int> m_queue_size;                  //记入当前队列的长度

        std::atomic<int> m_live_thread_num;             //当前存活的线程个数
        std::atomic<int> m_busy_thread_num;             //忙状态线程个数
        std::atomic<int> m_reduce_thread_num;           //需要削减的线程个数

        std::unique_ptr<IThreadTask>* m_task_queue;     //任务队列,每个任务是IThreadTask的派生类

        std::unique_ptr<std::pair<std::future<void>, std::thread>>* m_threads;      //用于保存线程中的工作线程
        std::unique_ptr<std::thread> m_adjust_thread;   //动态增减池中线程数的管理线程

        std::mutex m_task_queue_lock;
        std::condition_variable m_taskget_available;    //条件变量,让线程去获取task
        std::condition_variable m_taskadd_available;    //条件变量,允许添加task
};

template< class Rep, class Period >
ThreadPool::ThreadPool
( 
 int min_thread_num,
 int max_thread_num,
 int default_thread_vary,
 int max_queue_size,
 int waiting_tasks_danger_mark,
 std::chrono::duration<Rep, Period> const& adjust_interval_time
 ):
    MIN_THREAD_NUM(min_thread_num),
    MAX_THREAD_NUM(max_thread_num),
    DEFAULT_THREAD_VARY(default_thread_vary),
    MAX_QUEUE_SIZE(max_queue_size),
    WAITING_TASKS_DANGER_MARK(waiting_tasks_danger_mark),
    ADJUST_INTERVAL_TIME(adjust_interval_time),
    m_shutdown(true),
    m_queue_front(0),
    m_queue_rear(0),
    m_queue_size(0),
    m_live_thread_num(0),
    m_busy_thread_num(0),
    m_reduce_thread_num(0),
    m_task_queue(nullptr),
    m_threads(nullptr),
    m_adjust_thread(nullptr)
{
    assert(MIN_THREAD_NUM >= 0);
    assert(MIN_THREAD_NUM <= MAX_THREAD_NUM);
    assert(max_queue_size > 0);
}

ThreadPool::~ThreadPool()
{
    if (!m_shutdown)
    {
        ShutdownPool();
    }
    if (m_task_queue != nullptr)
    delete[] m_task_queue;
    if (m_threads != nullptr)
    delete[] m_threads;
}


void ThreadPool::StartPool()
{
    if (m_threads == nullptr)
        m_threads = new std::unique_ptr<std::pair<std::future<void>, std::thread>> [MAX_THREAD_NUM];

    if (m_task_queue == nullptr)
        m_task_queue = new std::unique_ptr<IThreadTask> [MAX_QUEUE_SIZE];

    m_shutdown = false;

    for (int i = 0; i < MIN_THREAD_NUM; ++i)
    {
        std::packaged_task<void()> thread_main(std::bind(&ThreadPool::StartThread, this));
        std::future<void> fut(thread_main.get_future());
        std::thread thr(std::move(thread_main));
#ifdef DEBUG
        {
            std::lock_guard<std::mutex> lg(print_mutex);
            std::cout<< "start thread "<< thr.get_id()<< std::endl;
        }
#endif
        std::unique_ptr<std::pair<std::future<void>, std::thread>> td_ptr(new std::pair<std::future<void>, std::thread> (std::move(fut), std::move(thr)));
        m_threads[i] = std::move(td_ptr);

        ++m_live_thread_num;
    }

    m_adjust_thread = std::unique_ptr<std::thread>(new std::thread (&ThreadPool::AdjustPool, this));

}


void ThreadPool::ShutdownPool()
{
    m_shutdown = true;

    m_adjust_thread->join();
    m_adjust_thread = nullptr;
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        if (m_threads[i] == nullptr)
            continue;

#ifdef DEBUG
        try{
            m_threads[i]->second.join();
            std::lock_guard<std::mutex> lg(print_mutex);
            std::cout<< "thread "<< i <<" joined"<< std::endl;
        }catch(...)
        {
            std::lock_guard<std::mutex> lg(print_mutex);
            std::cout<< "thread "<< i <<" join error "<< std::endl;
        }
#else
        m_threads[i]->second.join();
#endif

        m_threads[i] = nullptr;
    }

    m_live_thread_num.store(0);
    m_busy_thread_num.store(0);
}

void ThreadPool::SubmitTask(std::unique_ptr<IThreadTask> new_task)
{
    //m_task_queue_lock的上锁范围
    {
        std::unique_lock<std::mutex> ts_lock(m_task_queue_lock);
        while ((m_queue_size.load() == MAX_QUEUE_SIZE) && (!m_shutdown))
        {
            m_taskadd_available.wait(ts_lock);
        }
        if (m_shutdown)
        {
            return;
        }
        m_task_queue[m_queue_rear] = std::move(new_task);
        m_queue_rear = (m_queue_rear+1)%MAX_QUEUE_SIZE;
        ++m_queue_size;
    }   //m_task_queue_lock解锁
    m_taskget_available.notify_one();
}

void ThreadPool::StartThread()
{
    std::unique_ptr<IThreadTask> task;
    while (1)
    {
        //m_task_queue_lock的范围
        {
            std::unique_lock<std::mutex> tg_lock(m_task_queue_lock);
            while ((m_queue_size.load() == 0) && (!m_shutdown))
            {
                m_taskget_available.wait(tg_lock);

                if (m_reduce_thread_num > 0)
                {
                    --m_reduce_thread_num;
                    if (m_live_thread_num > MIN_THREAD_NUM)
                    {
                        --m_live_thread_num;
                        return;
                    }
                }
            }

            if (m_shutdown)
            {
                --m_live_thread_num;
                return;
            }

            task = std::move(m_task_queue[m_queue_front]);
            m_queue_front = (m_queue_front + 1) % MAX_QUEUE_SIZE;
            --m_queue_size;
        }   //m_task_queue_lock 解锁

        m_taskadd_available.notify_one();
        ++m_busy_thread_num;
        task->Execute();
        task = nullptr;
        std::this_thread::yield();
    }
}

void ThreadPool::AdjustPool()
{
    while (!m_shutdown)
    {
        std::this_thread::sleep_for(ADJUST_INTERVAL_TIME);

        int queue_size = m_queue_size.load();
        int live_thread_num = m_live_thread_num.load();
        int busy_thread_num = m_busy_thread_num.load();

        //如果等待任务数大于警戒线，并且存活的线程数小于最大线程个数，创建新线程
        if (queue_size >= WAITING_TASKS_DANGER_MARK && live_thread_num < MAX_THREAD_NUM)
        {
            int add = 0;
            for (int i = 0; i < MAX_THREAD_NUM && add < DEFAULT_THREAD_VARY
                    && m_live_thread_num.load() < MAX_THREAD_NUM; ++i)
            {
                if (m_threads[i] != nullptr) 
                {
                    if (m_threads[i]->first.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                        continue;
                    m_threads[i]->second.join();
                    m_threads[i] = nullptr;
                }
                std::packaged_task<void()> thread_main(std::bind(&ThreadPool::StartThread, this));
                std::future<void> fut(thread_main.get_future());
                std::thread thr(std::move(thread_main));

#ifdef DEBUG
                {
                    std::lock_guard<std::mutex> lg(print_mutex);
                    std::cout<< "start thread "<< thr.get_id()<< std::endl;
                }
#endif
                std::unique_ptr<std::pair<std::future<void>, std::thread>> td_ptr(new std::pair<std::future<void>, std::thread> (std::move(fut), std::move(thr)));
                m_threads[i] = std::move(td_ptr);

                ++add;
                ++m_live_thread_num;
            }

        }

        //如果忙碌线程个数小于空闲线程个数，并且线程个数大于最小线程个数，那么销毁空闲线程
        if (busy_thread_num*2 < live_thread_num && live_thread_num > MIN_THREAD_NUM)
        {
            m_reduce_thread_num.store(DEFAULT_THREAD_VARY);
            for (int i = 0; i < DEFAULT_THREAD_VARY; ++i)
            {
                m_taskadd_available.notify_one();
            }
        }
    }

#ifdef DEBUG
    {
        std::lock_guard<std::mutex> lg(print_mutex);
        std::cout<< "AdjustPool_thread_end\n"<< std::endl;
    }
#endif

}
