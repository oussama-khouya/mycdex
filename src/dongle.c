/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dongle.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

/*
** wait on condition variable until dongle is available and top of queue
*/
static int	wait_for_dongle(t_coder *coder, t_dongle *d)
{
	struct timespec	ts;

	while (!is_stopped(coder->data))
	{
		if (!(d->taken) && (top_request(&d->queue) == coder->id))
		{
			if (get_time_ms() >= d->available_at)
			{
				d->taken = 1;
				heap_pop_first(&d->queue);
				pthread_mutex_unlock(&d->mutex);
				return (1);
			}
			ts.tv_sec = d->available_at / 1000;
			ts.tv_nsec = (d->available_at % 1000) * 1000000;
			pthread_cond_timedwait(&d->cond, &d->mutex, &ts);
		}
		else
			pthread_cond_wait(&d->cond, &d->mutex);
	}
	remove_request(&d->queue, coder->id);
	pthread_mutex_unlock(&d->mutex);
	return (0);
}

/*
** takes one dongle: pushes request to queue and waits
*/
static int	take_dongle(t_coder *coder, int dongle_id)
{
	t_data		*data;
	t_dongle	*d;
	t_request	request;

	data = coder->data;
	d = &data->dongles[dongle_id];
	request.id = coder->id;
	request.arrival = get_time_ms();
	pthread_mutex_lock(&data->state_mutex);
	request.deadline = coder->last_compile + data->burnout;
	pthread_mutex_unlock(&data->state_mutex);
	pthread_mutex_lock(&d->mutex);
	heap_push(&d->queue, request);
	return (wait_for_dongle(coder, d));
}

/*
** if there is only one dongle one coder and already took one
** wait till simulation stops and give up
*/
static int	handle_single_coder(t_coder *coder)
{
	print_status(coder, "has taken a dongle");
	while (!is_stopped(coder->data))
		sleep_for_ms(1, coder->data);
	realease_dongles(coder);
	return (0);
}

/*
** take both dongles using resource hierarchy to prevent deadlock
*/
int	take_dongles(t_coder *coder)
{
	int	first;
	int	second;
	int	tmp;

	first = coder->left;
	second = coder->right;
	if (first > second)
	{
		tmp = first;
		first = second;
		second = tmp;
	}
	if (!take_dongle(coder, first))
		return (0);
	if (first == second)
		return (handle_single_coder(coder));
	if (!take_dongle(coder, second))
	{
		realease_dongles(coder);
		return (0);
	}
	print_status(coder, "has taken a dongle");
	print_status(coder, "has taken a dongle");
	return (1);
}

/*
** release both dongles and broadcast to waiting coders
*/
void	realease_dongles(t_coder *coder)
{
	int		i;
	int		id[2];
	t_data	*data;

	data = coder->data;
	id[0] = coder->left;
	id[1] = coder->right;
	i = 0;
	while (i < 2)
	{
		if (i == 1 && id[0] == id[1])
			break ;
		pthread_mutex_lock(&data->dongles[id[i]].mutex);
		if (data->dongles[id[i]].taken)
		{
			data->dongles[id[i]].taken = 0;
			data->dongles[id[i]].available_at = get_time_ms() + data->cooldown;
			pthread_cond_broadcast(&data->dongles[id[i]].cond);
		}
		pthread_mutex_unlock(&data->dongles[id[i]].mutex);
		i++;
	}
}
