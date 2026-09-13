/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   routine.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

/*
** compile phase:
** it print status and sleeps for the compiling time
*/
static void	compile(t_coder *coder)
{
	t_data	*data;

	data = coder->data;
	print_status(coder, "is compiling");
	sleep_for_ms(data->compile_time, data);
	pthread_mutex_lock(&data->state_mutex);
	coder->compile_count++;
	pthread_mutex_unlock(&data->state_mutex);
}

/*
** debug phase
*/
static void	debug(t_coder *coder)
{
	t_data	*data;

	data = coder->data;
	print_status(coder, "is debugging");
	sleep_for_ms(data->debug_time, data);
}

/*
** refactor phase
*/
static void	refactor(t_coder *coder)
{
	t_data	*data;

	data = coder->data;
	print_status(coder, "is refactoring");
	sleep_for_ms(data->refactor_time, data);
}

/*
** this is the function that will be executed for every coder thread
** running while not stopped:
** - the coder takes two dongles
** - update last_compile time
** - compile
** - release the dongles
** - check if stopped, debug and refactor
*/
void	*coder_routine(void *arg)
{
	t_coder	*coder;

	coder = (t_coder *)arg;
	while (!is_stopped(coder->data))
	{
		if (!take_dongles(coder))
			break ;
		pthread_mutex_lock(&coder->data->state_mutex);
		coder->last_compile = get_time_ms();
		pthread_mutex_unlock(&coder->data->state_mutex);
		compile(coder);
		realease_dongles(coder);
		if (is_stopped(coder->data))
			break ;
		debug(coder);
		if (is_stopped(coder->data))
			break ;
		refactor(coder);
	}
	return (NULL);
}
