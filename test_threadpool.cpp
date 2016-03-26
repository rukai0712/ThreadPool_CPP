#include<iostream>
#include<thread>
#include<functional>
#include"ThreadPool.hpp"

using namespace std;

int const MAX_THREAD_NUM = 25;
int const MIN_THREAD_NUM = 5;
int const DEFAULT_THREAD_VARY = 5;
int const QUEUE_MAX_SIZE = 20;
int const DANGER_MARK = 8;
std::chrono::seconds ADJUST_INTERVAL_TIME(1);

ThreadPool g_pool(MIN_THREAD_NUM, MAX_THREAD_NUM, DEFAULT_THREAD_VARY, QUEUE_MAX_SIZE, DANGER_MARK, ADJUST_INTERVAL_TIME);


extern std::mutex print_mutex;
void Printtask(int i)
{
    {
        std::lock_guard<std::mutex> lg(print_mutex);
//        printf("_______Task %2d \n", i);
    }
    sleep(1);
    {
        std::lock_guard<std::mutex> lg(print_mutex);
//        printf("Task_______ %2d \n", i);
    }
}

int main(int argc, char** argv)
{

    g_pool.StartPool();
    for (int i = 0; i < 40; ++i)
    {
        auto task_pair = CreateSimpleThreadTask(Printtask, i);
        {
            std::lock_guard<std::mutex> lg(print_mutex);
            printf("addtask %d\n", i);
        }
        g_pool.SubmitTask(std::move(task_pair.second));

    }

    //sleep(30);
    printf("ShutdownPool Send\n");
    //g_pool.ShutdownPool();
    return 0;
}

/*

   class X
   {
   public: 
   void num(void)
   {
   count = 10;
   }
   int count;
   };

   int main(int argc, char** argv)
   {
   X x;
   x.count = 1;
   thread t1(&X::num, &x);
   t1.join();
   cout<< x.count<< endl;

   return 0;
   }

   pthread_cond_t run;
   pthread_mutex_t mut;

   void* Thread_1(void* arg)
   {
   sleep(5);
   printf("Thread_1 after sleep\n");
   pthread_mutex_lock(&mut);
   pthread_cond_wait(&run, &mut);

   printf("Thread_1 unlocked");
   pthread_mutex_unlock(&mut);
   pthread_exit(nullptr);
   }


   int main(int argc, char** argv)
   {
   pthread_cond_init(&run, nullptr);
   pthread_mutex_init(&mut, nullptr);

   pthread_t tid;
   pthread_create(&tid, nullptr, Thread_1, nullptr);

   pthread_cond_signal(&run);
   printf("cond sended\n");

   sleep(10);
   printf("main after sleep\n");
   pthread_join(tid, nullptr);
   pthread_mutex_destroy(&mut);
   pthread_cond_destroy(&run);
   return 0;
   }
   */
