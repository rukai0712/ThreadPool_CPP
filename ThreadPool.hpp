#pragma once

#include<iostream>
#include<thread>
#include<atomic>

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
            :m_task(std::bind(std::forward<Function>(f), std::forward<Args>(args)...))
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
        std::packaged_task<result_type()> m_task;
};

/*
 * SimpleThreadTask 类的工厂函数
 * 返回future对象和unique_ptr<IThreadTask>对象
 */
    template< typename Function, typename... Args>
std::pair<std::future<typename std::result_of<Function()>::type>, std::unique_ptr<IThreadTask>> CreateSimpleThreadTask(Function&& f, Args&&... args )
{
    typedef typename std::result_of<Function>::type result_type;
    std::unique_ptr<IThreadTask> task = new SimpleThreadTask<result_type> (std::forward<Function>(f), std::forward<Args>(args)...);
    std::future<result_type> res(task->GetFuture());
    return std::make_pair(std::move(res), std::move(task));
}

class ThreadPool
{
    public:
        template< class Rep, class Period >
            ThreadPool
            (
             int max_thread_num,
             int min_thread_num,
             int waiting_tasks_danger_mark,
             int queue_max_size,
             std::chrono::duration<Rep, Period> const& adjust_interval_time
            );

        ~ThreadPool();

        void ShutdownPool();

        void SubmitTask(std::unique_ptr<IThreadTask> new_task);

    private:
        void StartThread();
        void AdjustPool();
    private:
        int const MAX_QUEUE_SIZE;                       //任务队列最大值
        int const MAX_THREAD_NUM;                       //线程池中最大线程数
        int const MIN_THREAD_NUM;                       //线程池中最小线程数
        int const WAITING_TASKS_DANGER_MARK;            //队列中允许未处理任务的数量，超过表示需要增加线程个数
        int const DEFAULT_THREAD_VARY;                  //默认线程增减的数量
        std::chrono::nanoseconds const ADJUST_INTERVAL_TIME;    //AdjustPool调整线程池的时间间隔

        bool m_shutdown;
        int m_queue_front;
        int m_queue_rear;
        std::atomic<int> m_queue_size;                  //记入当前队列的长度

        std::atomic<int> m_live_thread_num;             //当前存活的线程个数
        std::atomic<int> m_busy_thread_num;             //忙状态线程个数
        std::atomic<int> m_reduce_thread_num;           //需要削减的线程个数

        std::unique_ptr<IThreadTask>* m_task_queue;      //任务队列,每个任务是unique_ptr

        std::unique_ptr<std::pair<std::future<void>, std::thread>>* m_threads;

        std::thread m_adjust_thread;

        std::mutex m_task_queue_lock;
        std::condition_variable m_taskget_available;    //条件变量,让线程去获取task
        std::condition_variable m_taskadd_available;    //条件变量,允许添加task
};

template< class Rep, class Period >
ThreadPool::ThreadPool
( 
 int max_thread_num,
 int min_thread_num,
 int waiting_tasks_danger_mark,
 int queue_max_size,
 std::chrono::duration<Rep, Period> const& adjust_interval_time
 ):
    MAX_THREAD_NUM(max_thread_num),
    MIN_THREAD_NUM(min_thread_num),
    WAITING_TASKS_DANGER_MARK(waiting_tasks_danger_mark),
    QUEUE_MAX_SIZE(queue_max_size),
    ADJUST_INTERVAL_TIME(adjust_interval_time),
    m_shutdown(false),
    m_live_thread_num(0),
    m_busy_thread_num(0),
    m_reduce_thread_num(0),
    m_queue_size(0),
    m_queue_front(0),
    m_queue_rear(0)
{
    using namespace std;
    m_threads = new std::unique_ptr<std::pair<std::future<void>, std::thread>> [MAX_THREAD_NUM];
    m_task_queue = new std::unique_ptr<IThreadTask> [MAX_QUEUE_SIZE];

    for (int i = 0; i < min_thread_num; ++i)
    {
        std::packaged_task<void()> thread_main(std::bind(&ThreadPool::StartThread, this));
        std::future fut(thread_main.get_future());
        std::thread thr(std::move(thread_main));
        //for test
        cout<< "start thread "<< thr.get_id()<< endl;
        m_threads[i] = new pair<std::future<void>, std::thread> (std::move(fut), std::move(thr));
        ++m_live_thread_num;
    }

    std::thread adjust_thread(&ThreadPool::AdjustPool, this);
    m_adjust_thread = std::move(adjust_thread);
}

ThreadPool::~ThreadPool()
{
    ShutdownPool();
    delete[] m_task_queue;
    delete[] m_threads;
}

void ThreadPool::ShutdownPool()
{
    m_shutdown = true;

    m_adjust_thread.join();
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        if (m_threads[i] == nullptr)
            continue;
        m_threads[i]->second.join();
        m_threads[i] == nullptr;
    }
}

void ThreadPool::SubmitTask(std::unique_ptr<IThreadTask> new_task)
{
    //m_task_queue_lock的上锁范围
    {
        std::unique_lock ts_lock(m_task_queue_lock);
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
            std::unique_lock tg_lock(m_task_queue_lock);
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
    while (1)
    {
        std::this_thread::sleep_for(ADJUST_INTERVAL_TIME);
        int queue_size = m_queue_size.load();
        int live_thread_mun = m_live_thread_num.load();
        int busy_thread_mun = m_busy_thread_num.load();

        //如果等待任务数大于警戒线，并且存活的线程数小于最大线程个数，创建新线程
        if (queue_size >= WAITING_TASKS_DANGER_MARK && live_thread_mun < MAX_THREAD_NUM)
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
                    m_threads[i] == nullptr;
                }
                std::packaged_task<void()> thread_main(std::bind(&ThreadPool::StartThread, this));
                std::future fut(thread_main.get_future());
                std::thread thr(std::move(thread_main));
                m_threads[i] = new pair<std::future<void>, std::thread> (std::move(fut), std::move(thr));

                ++add;
                ++m_live_thread_num;
            }

        }

        //如果忙碌线程个数小于空闲线程个数，并且线程个数大于最小线程个数，那么销毁空闲线程
        if (busy_thread_mun*2 < live_thread_num && live_thread_num > MIN_THREAD_NUM)
        {
            m_reduce_thread_num.store(DEFAULT_THREAD_VARY);
            for (int i = 0; i < DEFAULT_THREAD_VARY; ++i)
            {
                m_taskadd_available.notify_one();
            }
        }
    }
}
