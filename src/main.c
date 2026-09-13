/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

/*
** Join monitor and coder threads
** - join the monitor thread
** - join coders threads
*/
static void	join_threads(t_data *data)
{
	int	i;

	pthread_join(data->monitor_thread, NULL);
	i = 0;
	while (i < data->coders_count)
	{
		pthread_join(data->coders[i].thread, NULL);
		i++;
	}
}

/*
** Main function:
** - memset the data memory
** - create coders thread
** - create the monitor threads
** - clean up
*/
int	main(int ac, char **av)
{
	t_data	data;
	int		i;

	memset(&data, 0, sizeof(data));
	if (!init_data(&data, ac, av))
	{
		write(2, "Error: Invalid arguments\n", 25);
		return (1);
	}
	i = 0;
	while (i < data.coders_count)
	{
		pthread_create(&data.coders[i].thread, NULL,
			coder_routine, &data.coders[i]);
		i++;
	}
	pthread_create(&data.monitor_thread, NULL, monitor, &data);
	join_threads(&data);
	cleanup(&data);
	return (0);
}
