#include "codexion.h"

// compare two requests who have the higher policy
// returns 1 if
static int higher(t_request a, t_request b, int policy)
{
	if (policy == LIFO)
	{
		if (a.arrival != b.arrival)
			return (a.arrival > b.arrival);
		return (a.id < b.id);
	}
	if (a.deadline != b.deadline)
		return (a.deadline < b.deadline);
	return (a.id > b.id);
}

static void swap(t_request *a, t_request *b)
{
	t_request tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void heap_push(t_heap *heap, t_request request)
{
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
			swap(&heap->items[0], &heap->items[1]);
	}
}

int top_request(t_heap *heap)
{
	if (heap->size == 0)
		return (-1);
	return (heap->items[0].id);
}

void heap_pop_first(t_heap *heap)
{
	if (heap->size == 0)
		return ;
	if (heap->size == 2)
		heap->items[0] = heap->items[1];
	// we decrease the size so what beyond it doest count anymore
	heap->size--;
}

void remove_request(t_heap *heap, int id)
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