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


static void	debug(t_coder *coder)
{
	t_data	*data;

	data = coder->data;
	print_status(coder, "is debugging");
	sleep_for_ms(data->debug_time, data);
}


static void	refactor(t_coder *coder)
{
	t_data	*data;

	data = coder->data;
	print_status(coder, "is refactoring");
	sleep_for_ms(data->refactor_time, data);
}

/*
check if coder finished its compiling cycle 
*/
static int	coder_is_finished(t_coder *coder)
{
	if (coder->compile_count >= coder->data->required)
	{
		pthread_mutex_lock(&coder->data->state_mutex);
		coder->finished = 1;
		pthread_mutex_unlock(&coder->data->state_mutex);
		return (1);
	}
	return (0);
}

/*
this is the function that will be executed for every coder thread
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
		release_dongles(coder);
		if (is_stopped(coder->data))
			break ;
		debug(coder);
		if (is_stopped(coder->data))
			break ;
		refactor(coder);
		/*
		check if coder finished its compiling cycle.
		*/
		if (coder_is_finished(coder))
			break ;
	}
	return (NULL);
}
