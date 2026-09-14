/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   init.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

/*
** parse the arguments from CLI and store them in the data struct
*/
static int	parse_args(t_data *data, int ac, char **av)
{
	if (ac != 9)
		return (0);
	data->coders_count = my_atoi(av[1]);
	data->burnout = my_atoi(av[2]);
	data->compile_time = my_atoi(av[3]);
	data->debug_time = my_atoi(av[4]);
	data->refactor_time = my_atoi(av[5]);
	data->required = my_atoi(av[6]);
	data->cooldown = my_atoi(av[7]);
	if (data->coders_count <= 0 || data->burnout < 0
		|| data->compile_time < 0 || data->debug_time < 0
		|| data->refactor_time < 0 || data->required < 0
		|| data->cooldown < 0)
		return (0);
	if (strcmp(av[8], "fifo") == 0)
		data->policy = FIFO;
	else if (strcmp(av[8], "edf") == 0)
		data->policy = EDF;
	else
		return (0);
	return (1);
}

/*
** initialize the dongles
** init the mutex and cond
*/
static void	init_dongles(t_data *data)
{
	int	i;

	i = 0;
	while (i < data->coders_count)
	{
		pthread_mutex_init(&data->dongles[i].mutex, NULL);
		pthread_cond_init(&data->dongles[i].cond, NULL);
		data->dongles[i].taken = 0;
		data->dongles[i].available_at = 0;
		data->dongles[i].queue.size = 0;
		data->dongles[i].queue.capacity = 2;
		data->dongles[i].queue.policy = data->policy;
		data->dongles[i].queue.items = malloc(sizeof(t_request) * 2);
		i++;
	}
}

/*
** initialize the coders
*/
static void	init_coders(t_data *data)
{
	int	i;

	i = 0;
	while (i < data->coders_count)
	{
		data->coders[i].finished = 0;
		data->coders[i].id = i + 1;
		data->coders[i].left = i;
		data->coders[i].right = (i + 1) % data->coders_count;
		data->coders[i].last_compile = data->start_time;
		data->coders[i].compile_count = 0;
		data->coders[i].data = data;
		i++;
	}
}

/*
** allocate the arr of dongles and coders and fill them
*/
int	init_data(t_data *data, int ac, char **av)
{
	if (!parse_args(data, ac, av))
		return (0);
	data->start_time = get_time_ms();
	data->stopped = 0;
	pthread_mutex_init(&data->print_mutex, NULL);
	pthread_mutex_init(&data->state_mutex, NULL);
	data->dongles = malloc(sizeof(t_dongle) * data->coders_count);
	data->coders = malloc(sizeof(t_coder) * data->coders_count);
	if (!data->dongles || !data->coders)
	{
		cleanup(data);
		return (0);
	}
	init_dongles(data);
	init_coders(data);
	return (1);
}

/*
** build the cleanup for failed allocation and exit
*/
void	cleanup(t_data *data)
{
	int	i;

	if (data->dongles)
	{
		i = 0;
		while (i < data->coders_count)
		{
			pthread_mutex_destroy(&data->dongles[i].mutex);
			pthread_cond_destroy(&data->dongles[i].cond);
			if (data->dongles[i].queue.items)
				free(data->dongles[i].queue.items);
			i++;
		}
		free(data->dongles);
		data->dongles = NULL;
	}
	if (data->coders)
	{
		free(data->coders);
		data->coders = NULL;
	}
	pthread_mutex_destroy(&data->print_mutex);
	pthread_mutex_destroy(&data->state_mutex);
}
