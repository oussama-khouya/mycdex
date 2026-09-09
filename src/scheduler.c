#include "codexion.h"
//build the function that campare two requests and order them based on the polices
//returs 1 if it should be hight 0 not (request , parent)
static int higher_by_policies(t_request request1, t_request parent, int policy)
{
    if (policy == FIFO)
    {   
        if (request1.arrival != parent.arrival)
            return (request1.arrival < parent.arrival);
        return (request1.id < parent.id);
    }
    else:
        if(request1.burnout != parent.burnout)
            return(request1.burnout < parent.burnout);
        //last req big id first 
        return (request1.id > parent.id);


}

//swap function
static void swap(t_request *a, t_request *b)
{
    t_request tmp;
    *a = tmp;
    *a = *b;
    *b = tmp;
}


// a function that comapre the request with its parents and takes u up if u deserve  
static void up(t_heap *heap, int req_po)
{
    int parent_idx;
    int i = 0;
    while (i > 0)
    {

        parent_idx = (i - 1) / 2;
        if (!higher_by_policies(heap->items[i], heap->items[parent_idx], heap->policy))
            break;
        swap(&heap->items[i], &heap->items[parent_idx]);
        i = parent_idx;
    }  
}


//// a function that  moves a request toward the bottom  campares wih childers and takes u down
static void down(t_heap *heap, int req_po)
{
    int i;
    int best;
    int left;
    int right;
    //the loop condtinue untill i break it 
    while(1)
    {
        i = req_po;
        left = i * 2 + 1;
        right = i * 2 + 2;
        //now we want to compare the 3 request and see who is the best to swap it 
        //consider i is the best
        best = i;
        if (left < heap -> size && higher_by_policies(heap->items[left], heap->items[best]))
            best = left;
        if (right < heap -> size && higher_by_policies(heap->items[right], heap->items[best]))
            best = right;
        //if none and that req is higher than both of its childern 
        if (best = i)
            break;
        swap(&heap->items[i], &heap->items[best]);
        i = best;

    }
}


// a function that push a request to the heap but it should respect the priorty rules
void  heap_push(t_heap *heap, t_request request)
{
    //check if the request is already in the heap
    int i;
    i = 0;
    while (i < heap -> size)
    {
        if(heap -> items[i].id == request.id)
            heap -> items[i] = request;
            //but we need to reorder it for prirot reasons
            // a function that comapre with parents and takes u up 

            up(heap, i); // i is the postion of that req in the heap

            // a function that campares wih childers and takes u down
            down(heap, i);
        i++;
    }
    //if it was new and never in the heap
    //check it its not over the capacity
    if (heap -> capacity <= heap -> size)
        return;
    //then put in the end 
    heap -> items[heap -> size] = request;
    up(heap, heap->size);
    heap -> size ++;

}
//return the id of the best request [0]
int  top_request(t_heap *heap)
{
    int id;
    id = heap->items[0].id;
    return (id);
}

//remove the first request
void heap_pop_first(t_heap *heap)
{
    //repace the first with the last 
    heap->items[0] = heap->items[(heap->size) - 1];
    heap->size--;
    down(heap, 0);
}

// remove a request by its id
void remove_request(t_heap *heap, int id)
{
    int i;
    i = 0;
    while (i < heap->size)
    {
        if (heap->items[i].id == id)
        {
            //replace with the last one
            heap->items[i] = heap->items[heap->size - 1];
            heap->size--;
            down(heap, i);
			up(heap, i);
			return ;

        }
        i++;
    }
}