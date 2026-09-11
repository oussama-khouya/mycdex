#ifndef CODEXION_H
# define CODEXION_H

#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"
#include "pthread.h"
#include <sys/time.h>
#include <string.h>

// Forward declaration of shared data struct
typedef struct s_data t_data;

// coder struct
typedef struct s_coder
{
    int id;
    int right;
    int left;
    int compile_count;
    long last_compile;
    pthread_t thread;
    t_data *data;
    
    
} t_coder;

//define policies

# define FIFO 0
# define EDF 1

typedef struct s_request
{
    int id;
    long arrival; //fifo
    long deadline; // EDF

} t_request;

typedef struct s_heap
{
    t_request *items; //array of request
    int capacity; // how much it takes
    int size; // its size
    int policy; //fifo EDF

} t_heap;

// dongle struct
typedef struct s_dongle
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int taken;
    long available_at;
    t_heap queue; //priorty heap

} t_dongle;




// build the data struct that is shared data 
typedef struct s_data
{
    int coders_count;
    int required;
    int policy;
    int stopped;
    // time related
    long start_time;
    long compile_time;
    long debug_time;
    long refactor_time;
    long burnout;
    long cooldown;

    // metux to protex data state
    pthread_mutex_t state_mutex;
    pthread_mutex_t print_mutex;

    // coder and dongles arr

    t_dongle *dongles;
    t_coder *coders;
    pthread_t monitor_thread;

} t_data;

// functions 
int my_atoi(const char *str);
void cleanup(t_data *data);
int init_data(t_data *data, int ac, char **av);
void print_status(t_coder *coder, const char *msg);
int is_stopped(t_data *data);
void heap_push(t_heap *heap, t_request request);
int  top_request(t_heap *heap);
void heap_pop_first(t_heap *heap);
void remove_request(t_heap *heap, int id);
void *coder_routine(void *arg);

int take_dongles(t_coder *coder);
void    realease_dongles(t_coder *coder);
void *monitor(void *arg);

long get_time_ms(void);
void sleep_for_ms(long mss, t_data *data);

#endif