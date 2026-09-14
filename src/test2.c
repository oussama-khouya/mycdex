#include <stdio.h>
#include <pthread.h>

int count = 0;
pthread_mutex_t lock;

void *increase(void *arg)
{
    int *c = (int *)arg;
    int i = 0;
    while (i < 100000)
    {
        i++;
        pthread_mutex_lock(&lock);    // Acquire the lock
        *c = *c + 1;                  // Critical section (safe from race conditions)
        pthread_mutex_unlock(&lock);  // Release the lock
    }
    return NULL;
}

int main()
{
    pthread_t thread1;
    pthread_t thread2;

    // Initialize the mutex
    pthread_mutex_init(&lock, NULL);

    // Create threads
    pthread_create(&thread1, NULL, increase, &count);
    pthread_create(&thread2, NULL, increase, &count);

    // Wait for threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // Clean up the mutex
    pthread_mutex_destroy(&lock);

    printf("count = %d\n", count); // Expected output: 200000
    return 0;
}
