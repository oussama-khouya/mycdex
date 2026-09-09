#include "codexion.h"

// function that cnv time to ms
long get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    //convert it to ms

    return ((tv.tv_sec * 1000) + (tv.tv_usec) / 1000);
}

// we need a function that sleeps for an amount time 
// for comiling for debuging for ...
//why noy sleep nn cause 

void sleep_for_ms(long mss, t_data *data)
{
    long start;
    start = get_time_ms();
    while(!is_stopped(data))
    {
        if (get_time_ms - start >= mss)
            break;
        usleep(250);
    }
}

// we need a function that checks if the simulation stopped

int is_stopped(t_data *data)
{
    int res;
    pthread_mutex_lock(&data->state_mutex)
    res = data -> stopped;
    pthread_mutex_unlock(&data->state_mutex)
    return res;
}






//build atoi to conv av to int
// but this atoi validate the arg also
int my_atoi(const char *str)
{
    // long to catch long numbers
    long res;
    int i ;

    i = 0;
    while (str[i] == ' ' || str[i] >= 9 && str[i] <= 13)
        i++;
    
    if (str[i] == '+')
		i++;
    // if we reach the end before findin no number "    +"
    if (!str[i])
        return (-1);
    
    while (str[i])
    {
        //if they are not between 0 9 regect
        if (str[i] < '0'|| str[i] > '9')
            return (-1);
        res = res * 10 + (str[i] - '0');
        if (res > 2147483647)
			return (-1);

    }
    return ((int)res);
    
}
// function that print coder status 
void print_status(t_coder *coder, const char *msg)
{
    
    long timestaps;
    t_data *data = coder -> data;
    // u want to print lock the print metux
    pthread_mutex_lock(&data -> print_mutex);

    while(!data -> stopped)
    {
        // we need a function that give us the exact time with ms
        timestaps = get_time_ms() - data -> start_time;
        printf("%ld %d %s\n", timestaps, coder -> id, msg);
    }
    pthread_metux_unlock(&data -> print_mutex);
}