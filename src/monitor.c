/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   monitor.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

/*
** Checks if all coders have finished required number of compilations.
*/
static int	all_finished_compiling(t_data *data)
{
	int	i;

	i = 0;
	while (i < data->coders_count)
	{
		pthread_mutex_lock(&data->state_mutex);
		if (data->coders[i].finished == 0)
		{
			pthread_mutex_unlock(&data->state_mutex);
			return (0);
		}
		pthread_mutex_unlock(&data->state_mutex);
		i++;
	}
	return (1);
}

/*
** Wakes up coders that were waiting when simulation stops.
*/
static void wake_sleep_coders(t_data *data)
{
	int i;

	i = 0;
	pthread_mutex_lock(&data->state_mutex);
	while (i < data->coders_count)
	{
		pthread_cond_broadcast(&data->dongles[i].cond);
		i++;
	}
	pthread_mutex_unlock(&data->state_mutex);
}

static int	check_coder_burnout(t_data *data, int i)
{
	long	time;

	pthread_mutex_lock(&data->state_mutex);
	if (data->coders[i].finished == 1)
	{
		pthread_mutex_unlock(&data->state_mutex);
		return (0);
	}
	if (get_time_ms() - data->coders[i].last_compile >= data->burnout)
	{
		data->stopped = 1;
		time = get_time_ms() - data->start_time;
		pthread_mutex_unlock(&data->state_mutex);
		pthread_mutex_lock(&data->print_mutex);
		printf("%ld %d burned out\n", time, data->coders[i].id);
		pthread_mutex_unlock(&data->print_mutex);
		wake_sleep_coders(data);
		return (1);
	}
	pthread_mutex_unlock(&data->state_mutex);
	return (0);
}

/*
** Checks if any coder burned out.
*/
static int	is_burnout(t_data *data)
{
	int	i;

	i = 0;
	while (i < data->coders_count)
	{
		if (check_coder_burnout(data, i))
			return (1);
		i++;
	}
	return (0);
}

/*
** Monitor thread routine: checks burnout and completion.
*/
void	*monitor(void *arg)
{
	t_data	*data;

	data = (t_data *)arg;
	while (!is_stopped(data))
	{
		if (is_burnout(data))
			break ;
		if (all_finished_compiling(data))
		{
			pthread_mutex_lock(&data->state_mutex);
			data->stopped = 1;
			pthread_mutex_unlock(&data->state_mutex);
			wake_sleep_coders(data);
			break ;
		}
		usleep(250);
	}
	return (NULL);
}
