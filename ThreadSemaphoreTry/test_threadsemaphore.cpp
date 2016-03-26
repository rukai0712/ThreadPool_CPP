#include<iostream>
#include<vector>
#include<thread>
#include<mutex>
#include"ThreadSemaphore.hpp"

using namespace std;


struct Task
{
    Task(int val)
        :value(val)
    {}

    int value;
};

int const MAX_QUEUE_SIZE = 10;

ThreadSemaphore queue_empty_num(MAX_QUEUE_SIZE);
ThreadSemaphore queue_full_num(0);
mutex queue_lock;
Task** task_queue;
int task_head;
int task_rear;

void Consumer(void)
{
    sleep(1);
    cout<< "Consumer start "<< std::this_thread::get_id()<< endl;
    while (1)
    {
        Task* getTask;
        queue_full_num.Wait();
        {
            std::lock_guard<std::mutex> lock(queue_lock);
            getTask = task_queue[task_head];
            task_queue[task_head] = nullptr;
            task_head = (task_head + 1) % MAX_QUEUE_SIZE;
            printf("Consumer consume %d\n", getTask->value);
        }
        queue_empty_num.Post();
        delete getTask;
    }
}

void Producer(void)
{
    int count = 0;
    cout<< "Producer start "<< std::this_thread::get_id()<< endl;
    while (1)
    {
        
        sleep(1);
        Task* nwTask = new Task(count);
        ++count;

        queue_empty_num.Wait();
        {
            std::lock_guard<std::mutex> lock(queue_lock);
            task_queue[task_rear] = nwTask;
            task_rear = (task_rear + 1) % MAX_QUEUE_SIZE;
        }
        printf("Producer produce %d\n", nwTask->value);
        queue_full_num.Post();
    }
}

int main(int argc, char** argv)
{
    task_queue = new Task*[MAX_QUEUE_SIZE];
    task_head = 0;
    task_rear = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back(Consumer);
    }
    Producer();
    return 0;
}
