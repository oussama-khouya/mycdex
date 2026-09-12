//lets build a data race 
//where two threads access to the same shared data at the same time atlest one write 
//it result an undefined behavouir 

#include <pthread.h>
#include <stdio.h>

//global varible shared 
int count = 0;
pthread_mutex_t lock;

//build the increase function
void *increase(void *arg)
{
    int i = 0;
    while (i < 100000)
    {
        i++;

        pthread_mutex_lock(&lock);
        count++;
        pthread_mutex_unlock(&lock);
    }
    return (NULL);
}

//BUILD the main
int main(void)
{
    //create the threads
    pthread_t thread1;
    pthread_t thread2;

    //initalse the mutex

    pthread_mutex_init(&lock, NULL);

    //create the threads
    pthread_create(&thread1, NULL, increase, NULL);
    pthread_create(&thread2, NULL, increase, NULL);

    //join the threads 
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("the value of count = %d", count);


}
