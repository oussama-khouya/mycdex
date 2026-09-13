/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

/*
** function that converts time to ms
*/
long	get_time_ms(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((tv.tv_sec * 1000) + (tv.tv_usec / 1000));
}

/*
** sleeps for an amount of time in ms
*/
void	sleep_for_ms(long mss, t_data *data)
{
	long	start;

	start = get_time_ms();
	while (!is_stopped(data))
	{
		if (get_time_ms() - start >= mss)
			break ;
		usleep(250);
	}
}

/*
** checks if the simulation is stopped
*/
int	is_stopped(t_data *data)
{
	int	res;

	pthread_mutex_lock(&data->state_mutex);
	res = data->stopped;
	pthread_mutex_unlock(&data->state_mutex);
	return (res);
}

/*
** atoi implementation with validation
** long to catch overflow numbers
*/
int	my_atoi(const char *str)
{
	long	res;
	int		i;

	i = 0;
	res = 0;
	while (str[i] == ' ' || (str[i] >= 9 && str[i] <= 13))
		i++;
	if (str[i] == '+')
		i++;
	if (!str[i])
		return (-1);
	while (str[i])
	{
		if (str[i] < '0' || str[i] > '9')
			return (-1);
		res = res * 10 + (str[i] - '0');
		if (res > 2147483647)
			return (-1);
		i++;
	}
	return ((int)res);
}

/*
** print coder status with timestamp under print and state mutexes
*/
void	print_status(t_coder *coder, const char *msg)
{
	long	timestaps;
	t_data	*data;

	data = coder->data;
	pthread_mutex_lock(&data->print_mutex);
	pthread_mutex_lock(&data->state_mutex);
	if (!data->stopped)
	{
		timestaps = get_time_ms() - data->start_time;
		printf("%ld %d %s\n", timestaps, coder->id, msg);
	}
	pthread_mutex_unlock(&data->state_mutex);
	pthread_mutex_unlock(&data->print_mutex);
}
