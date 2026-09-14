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
** Checks if a dongle can be taken by coder
**not taken + cooledown + and top request
*/
static int	can_take_dongle(t_coder *c, int id)
{
	t_dongle	*d;

	d = &c->data->dongles[id];
	if (d->taken || get_time_ms() < d->available_at)
		return (0);
	/*it also should be the top request*/
	return (top_request(&d->queue) == c->id);
}

/*
** waits until both dongles f and s are ready to be taken simuli
** if a dongle is on cooldown, timedwaits until available_at.
** else waits on condvar till wake up
*/
static void	wait_both(t_coder *coder, int first, int second)
{
	t_dongle		*d;
	struct timespec	ts;
	long			now;

	while (!coder->data->stopped && (!can_take_dongle(coder, first)
			|| !can_take_dongle(coder, second)))
	{
		d = &coder->data->dongles[first];
		now = get_time_ms();
		/*then wait for the second*/
		if (!d->taken && now >= d->available_at
			&& top_request(&d->queue) == coder->id)
			d = &coder->data->dongles[second];
		/* on cooldown */
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

static void	push_both(t_coder *coder, int first, int second)
{
	t_request	req;

	req.id = coder->id;
	req.arrival = get_time_ms();
	req.deadline = coder->last_compile + coder->data->burnout;
	pthread_mutex_lock(&coder->data->state_mutex);
	heap_push(&coder->data->dongles[first].queue, req);
	heap_push(&coder->data->dongles[second].queue, req);
}

/*
**i changed to take both dongles to avoid hold and wait 
**now it try to aquire both dongle at the same time
*/
int	take_dongles(t_coder *coder)
{
	int			first;
	int			second;
	int			ok;
	t_dongle	*d;

	first = coder->left;
	second = coder->right;
	if (first == second)
		return (handle_single_coder(coder));
	d = coder->data->dongles;
	push_both(coder, first, second);
	wait_both(coder, first, second);
	remove_request(&d[first].queue, coder->id);
	remove_request(&d[second].queue, coder->id);
	ok = !coder->data->stopped;
	d[first].taken = ok;
	d[second].taken = ok;
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
	t_dongle	*first;
	t_dongle	*second;
	long		avail;

	first = &coder->data->dongles[coder->left];
	second = &coder->data->dongles[coder->right];
	avail = get_time_ms() + coder->data->cooldown;
	pthread_mutex_lock(&coder->data->state_mutex);
	if (first->taken)
	{
		first->taken = 0;
		first->available_at = avail;
		pthread_cond_broadcast(&first->cond);
	}
	if (second->taken && coder->left != coder->right)
	{
		second->taken = 0;
		second->available_at = avail;
		pthread_cond_broadcast(&second->cond);
	}
	pthread_mutex_unlock(&coder->data->state_mutex);
}
