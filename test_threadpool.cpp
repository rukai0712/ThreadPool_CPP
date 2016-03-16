#include<iostream>
#include<thread>
#include<functional>
#include<pthread.h>

using namespace std;
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
*/

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
