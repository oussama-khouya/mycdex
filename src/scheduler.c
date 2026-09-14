/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   scheduler.c                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: okhouya <okhouya@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/13 01:00:00 by okhouya           #+#    #+#             */
/*   Updated: 2026/09/13 01:00:00 by okhouya          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	higher(t_request a, t_request b, int policy)
{
	if (policy == FIFO)
	{
		if (a.arrival != b.arrival)
			return (a.arrival < b.arrival);
		return (a.id < b.id);
	}
	if (a.deadline != b.deadline)
		return (a.deadline < b.deadline);
	return (a.id > b.id);
}

void	heap_push(t_heap *heap, t_request request)
{
	t_request	tmp;

	if (heap->size == 0)
	{
		heap->items[0] = request;
		heap->size = 1;
		return ;
	}
	if (heap->size == 1)
	{
		heap->items[1] = request;
		heap->size = 2;
		if (higher(heap->items[1], heap->items[0], heap->policy))
		{
			tmp = heap->items[0];
			heap->items[0] = heap->items[1];
			heap->items[1] = tmp;
		}
	}
}

int	top_request(t_heap *heap)
{
	if (heap->size == 0)
		return (-1);
	return (heap->items[0].id);
}

void	heap_pop_first(t_heap *heap)
{
	if (heap->size == 0)
		return ;
	if (heap->size == 2)
		heap->items[0] = heap->items[1];
	heap->size--;
}

/*
** Removes a specific request from the heap by coder id.
*/
void	remove_request(t_heap *heap, int id)
{
	if (heap->size == 0)
		return ;
	if (heap->items[0].id == id)
	{
		if (heap->size == 2)
			heap->items[0] = heap->items[1];
		heap->size--;
	}
	else if (heap->size == 2 && heap->items[1].id == id)
		heap->size--;
}
