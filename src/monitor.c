#include "codexion.h"

//this is the main monitor thread function that checks 
//deos the coder bunout
//and does all coder finishs the compilition  and it stop it or not 





static int all_finished_compiling(t_data *data)
{
    int i;
    i = 0;
    while(i < data -> coders_count)
    {
        pthread_mutex_lock(&data->state_mutex);
        if (data->coders[i].compile_count < data->required)
        {
            pthread_mutex_unlock(&data->state_mutex);
            return (0);
        }
        pthread_mutex_unlock(&data->state_mutex);
        i++;

    }
    return (1);
}

//THE CODER that were a sleep when the somulation wanted to stop they should wake up
static void wake_sleep_coders(t_data *data)
{
    int i;

    i = 0;
    while(i < data -> coders_count)
    {
        pthread_mutex_lock(&data->dongles[i].mutex);
        pthread_cond_broadcast(&data->dongles[i].cond);
        pthread_mutex_unlock(&data->dongles[i].mutex);
        i++;
    }
}


static int is_burnout(t_data *data)
{
    int i = 0;
    while(i < data->coders_count)
    {
        pthread_mutex_lock(&data->state_mutex);
        //if the coder is burnedout
        if(get_time_ms() - data->coders[i].last_compile > data->burnout)
        {
            data -> stopped = 1;
            pthread_mutex_unlock(&data->state_mutex);
            pthread_mutex_lock(&data->print_mutex);
            printf("%ld %d burned out\n", get_time_ms() - data->start_time, data->coders[i].id);
            pthread_mutex_unlock(&data->print_mutex);
            //wake up the sleep coders
            wake_sleep_coders(data);
            return (1);

        }
        pthread_mutex_unlock(&data->state_mutex);
        i++;

    }
    return (0);
}


void *monitor(void *arg)
{
    t_data *data;
    data = arg;

    while (!is_stopped(data))
    {
        if(is_burnout(data))
            break;
        if(all_finished_compiling(data))
        {
            pthread_mutex_lock(&data->state_mutex);
            data -> stopped = 1;
            pthread_mutex_unlock(&data->state_mutex);

            //NOW AGAIN WAKE THE SLEEP CODERS
            wake_sleep_coders(data);
            break;
        }
        //to not waste the cpu on looping sleep some time 
        usleep(250);
    }
    return (NULL);
}
