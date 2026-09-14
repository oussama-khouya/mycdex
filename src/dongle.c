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

static int	can_take_d(t_coder *c, int id)
{
	t_dongle	*d;
	int			o_id;
	int			o_d;
	t_dongle	*od;

	d = &c->data->dongles[id];
	if (d->taken || get_time_ms() < d->available_at)
		return (0);
	if (top_request(&d->queue) == c->id)
		return (1);
	if (c->id == id + 1)
		o_id = (id == 0 ? c->data->coders_count : id);
	else
		o_id = id + 1;
	o_d = c->data->coders[o_id - 1].left;
	if (o_d == id)
		o_d = c->data->coders[o_id - 1].right;
	od = &c->data->dongles[o_d];
	return (od->taken || get_time_ms() < od->available_at);
}
//get
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

int	take_dongles(t_coder *coder)
{
	int			f;
	int			s;
	int			ok;
	t_dongle	*d;

	if (coder->left == coder->right)
		return (handle_single_coder(coder));
	f = coder->left < coder->right ? coder->left : coder->right;
	s = coder->left + coder->right - f;
	d = coder->data->dongles;
	push_both(coder, f, s);
	wait_both(coder, f, s);
	remove_request(&d[f].queue, coder->id);
	remove_request(&d[s].queue, coder->id);
	ok = !coder->data->stopped;
	if (ok)
		d[f].taken = (d[s].taken = 1);
	pthread_mutex_unlock(&coder->data->state_mutex);
	if (ok)
		return (print_status(coder, "has taken a dongle"),
			print_status(coder, "has taken a dongle"), 1);
	return (0);
}

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
