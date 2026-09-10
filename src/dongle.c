//takes dongles 
#include "codexion.h"

// a function that takes just one dongle left or right dongle of the coder
static int take_dongle(t_coder *coder, int dongle_id)
{
    //request fullfill
    t_data *data;
    t_dongle *d;
    t_request request;
    //this struct needs by the wait pthread
    struct timespec ts;

    
    data = coder -> data;
    d = &data -> dongles[dongle_id];

    request.id = coder->id;
    request.arrival = get_time_ms();

    pthread_mutex_lock(&data->state_mutex);
    request.deadline = coder->last_compile + data->burnout;
    pthread_mutex_unlock(&data->state_mutex);

    //push the request
    pthread_mutex_lock(&d->mutex);
    heap_push(&d->queue, request);
    //countinue
    while(!is_stopped(data))
    {
           //check if its not taken and aslo if that request is top request 
        if(!(d->taken) && d->queue.size > 0 && (top_request(&d->queue) == coder->id))
        {
            //check cooldown peroid
            if (get_time_ms() >= d->available_at)
            {
                d->taken = 1;
                //take request of the dong heap
                heap_pop_first(&d->queue);
                pthread_mutex_unlock(&d->mutex);
                print_status(coder, "has taken a dongle");
                return (1);
            }
            //if still not avaible 
            ts.tv_sec = d->available_at / 1000L;
            ts.tv_nsec = (d->available_at % 1000L) * 1000000L;
            pthread_cond_timedwait(&d->cond, &d->mutex, &ts);

        }
        else
            //if its taken
            //wait on that cond variable until somth change for that dongle 
            pthread_cond_wait(&d->cond, &d->mutex);
    }
    //if the sumilation stopped
    remove_request(&d->queue, coder->id);
    pthread_mutex_unlock(&d->mutex);
    return (0);

}



int take_dongles(t_coder *coder)
{
    int first;
    int second;
    int tmp;

    first = coder->left;
    second = coder->right;

    //put just in order
    if (first > second)
    {
        tmp = first;
        first = second;
        second = tmp;
    }
    if(!take_dongle(coder, first))
        return(0);
    //if there is only one dongle one coder and already i took one
    // so just wait till summulation and give up
    if(first == second)
    {
        while(!is_stopped(coder->data))
            sleep_for_ms(1, coder->data);
        pthread_mutex_lock(&coder->data->dongles[first].mutex);
        coder->data->dongles[first].taken = 0;
        pthread_cond_broadcast(&coder->data->dongles[first].cond);
        pthread_mutex_unlock(&coder->data->dongles[first].mutex);
        return (0);
    }
    if(!take_dongle(coder, second))
    {    
        pthread_mutex_lock(&coder->data->dongles[first].mutex);
        coder->data->dongles[first].taken = 0;
        coder->data->dongles[first].available_at = get_time_ms() + coder->data->cooldown;
        pthread_cond_broadcast(&coder->data->dongles[first].cond);
        pthread_mutex_unlock(&coder->data->dongles[first].mutex);
        return (0);
    }
    return (1);


}

void    realease_dongles(t_coder *coder)
{
    int i;
    int id;
    t_data *data;
    
    data = coder -> data;
    i = 0;
    while (i < 2)
    {
        if (i == 0)
            id = coder->left;
        else
            id = coder->right;

        //if we have only one dongle we took only one should we already relase only one
        if(i == 1 && coder->left == coder->right)
            break;
        
        pthread_mutex_lock(&data->dongles[id].mutex);
        if(data->dongles[id].taken)
        {
            data ->dongles[id].taken = 0;
            data ->dongles[id].available_at= get_time_ms() + data-> cooldown;
            //now when its free broadcast to all other waiting threads with that cond variable
            pthread_cond_broadcast(&data->dongles[id].cond);
        }
        pthread_mutex_unlock(&data->dongles[id].mutex);
        i++;



    }
}
