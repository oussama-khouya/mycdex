#include "codexion.h"
// this is the next file coder routine

//compile
static void compile(t_coder *coder)
{
    t_data *data = coder -> data;
    //it print status
    print_status(coder, "is_compiling");
    //it sleeps for the compiling time
    sleep_for_ms(data -> compile_time, data);
    pthread_mutex_lock(&data -> state_mutex);
    coder -> compile_count++;
    pthread_mutex_unlock(&data -> state_mutex);
    
}

//debug

static void debug(t_coder *coder)
{
    data *data = coder -> data;
    print_status(coder, "is_debuging");
    sleep_for_ms(data -> debug_time, data);

}

//refactor
static void refactor(t_coder *coder)
{
    data *data = coder -> data;
    print_status(coder, "is_refactoring");
    sleep_for_ms(data -> refactor_time, data);
}

// this is the function that will be excuted for every coder thread that the thread will excute

void *coder_routine(void *arg)
{
    t_coder *coder;
    coder = arg;
    //ruuning while not stopped
    while(!is_stopped(coder -> data))
    {
        //the coder takes two dongles
        if(!take_dongles(coder));
            break;
        //update last_compile time
        pthread_mutex_lock(coder->data->state_mutex);
        coder->last_compile = get_time_ms;
        //compile
        compile(coder);
        //realease the dongles
        realease_dongles(coder);
        //check if stopped
        
        if (stopped(coder->data))
			break ;
		debug(coder);
		if (stopped(coder->data))
			break ;
		refactor(coder);
    }
    return (NULL);


}