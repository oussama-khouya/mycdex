#include "codexion.h"

int main(int ac, char **av)
{
    t_data data;
    int i;

    //memset the data memory 
    memset(&data, 0, sizeof(data));
    if (!init_data(&data, ac, av))
    {
        write(2, "Error: Invalid arguments\n", 25);
		return (1);
    }
    data.start_time = get_time_ms();
    i = 0;
    while(i < data.coders_count)
    {
        data.coders[i].last_compile = data.start_time;
        i++;
    }
    //create coders thread
    i = 0;
    while(i < data.coders_count)
    {
        pthread_create(&data.coders[i].thread, NULL, coder_routine, &data.coders[i]);
        i++;
    }

    //create the monitor threads 
    pthread_create(&data.monitor, NULL, monitor, &data);

    //join the monitor thread
    pthread_join(data.monitor, NULL);

    //join coders threads
    i = 0;
    while(i < data.coders_count)
    {
        pthread_join(data.coders[i].thread, NULL);
        i++;
    }
    //clean up 
    cleanup(&data);
    return (0);
}