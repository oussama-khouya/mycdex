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
** Checks if a dongle can be taken by coder:
** Must not be currently taken, cooldown must have elapsed,
** and coder must be at the top of the priority queue (FIFO / EDF).
*/
static int	can_take_d(t_coder *c, int id)
{
	t_dongle	*d;

	d = &c->data->dongles[id];
	if (d->taken || get_time_ms() < d->available_at)
		return (0);
	return (top_request(&d->queue) == c->id);
}

/*
** Waits until both dongles 'f' and 's' are ready to be taken simultaneously.
** If a dongle is on cooldown, timedwaits until available_at.
** Otherwise waits on condvar until woken up when a dongle is released.
*/
static void	wait_both(t_coder *coder, int f, int s)
{
	t_dongle		*d;
	struct timespec	ts;
	long			now;

	while (!coder->data->stopped && (!can_take_d(coder, f)
			|| !can_take_d(coder, s)))
	{
		d = &coder->data->dongles[f];
		now = get_time_ms();
		if (!d->taken && now >= d->available_at
			&& top_request(&d->queue) == coder->id)
			d = &coder->data->dongles[s];
		if (!d->taken && now < d->available_at)
		{
			ts.tv_sec = d->available_at / 1000;
			ts.tv_nsec = (d->available_at % 1000) * 1000000;
			pthread_cond_timedwait(&d->cond, &coder->data->state_mutex, &ts);
		}
		else
			pthread_cond_wait(&d->cond, &coder->data->state_mutex);
	}
}

/*
** Enqueues the coder's request into both dongles' priority queues
** under state_mutex. Contains coder ID, arrival time, and burnout deadline.
*/
static void	push_both(t_coder *coder, int f, int s)
{
	t_request	req;

	req.id = coder->id;
	req.arrival = get_time_ms();
	req.deadline = coder->last_compile + coder->data->burnout;
	pthread_mutex_lock(&coder->data->state_mutex);
	heap_push(&coder->data->dongles[f].queue, req);
	heap_push(&coder->data->dongles[s].queue, req);
}

/*
** Atomically acquires both dongles at once to eliminate hold-and-wait starvation.
** Enqueues to both queues, waits until both are available, then claims both.
** If only 1 coder exists, delegates to handle_single_coder.
*/
int	take_dongles(t_coder *coder)
{
	int			f;
	int			s;
	int			ok;
	t_dongle	*d;

	f = coder->left;
	s = coder->right;
	if (f == s)
		return (handle_single_coder(coder));
	d = coder->data->dongles;
	push_both(coder, f, s);
	wait_both(coder, f, s);
	remove_request(&d[f].queue, coder->id);
	remove_request(&d[s].queue, coder->id);
	ok = !coder->data->stopped;
	d[f].taken = ok;
	d[s].taken = ok;
	pthread_mutex_unlock(&coder->data->state_mutex);
	if (!ok)
		return (0);
	print_status(coder, "has taken a dongle");
	print_status(coder, "has taken a dongle");
	return (1);
}

/*
** Releases both dongles, sets their cooldown timestamp (now + cooldown),
** and broadcasts their condition variables to wake up waiting coders.
*/
void	release_dongles(t_coder *coder)
{
	t_dongle	*df;
	t_dongle	*ds;
	long		avail;

	df = &coder->data->dongles[coder->left];
	ds = &coder->data->dongles[coder->right];
	avail = get_time_ms() + coder->data->cooldown;
	pthread_mutex_lock(&coder->data->state_mutex);
	if (df->taken)
	{
		df->taken = 0;
		df->available_at = avail;
		pthread_cond_broadcast(&df->cond);
	}
	if (ds->taken && coder->left != coder->right)
	{
		ds->taken = 0;
		ds->available_at = avail;
		pthread_cond_broadcast(&ds->cond);
	}
	pthread_mutex_unlock(&coder->data->state_mutex);
}
