// takes dongles
#include "codexion.h"

// a function that takes just one dongle left or right dongle of the coder
static int take_dongle(t_coder *coder, int dongle_id) {
  // request fullfill
  t_data *data;
  t_dongle *d;
  t_request request;
  // this struct needs by the wait pthread
  struct timespec ts;

  data = coder->data;
  d = &data->dongles[dongle_id];

  request.id = coder->id;
  request.arrival = get_time_ms();

  //printf("request by coder %d for dongle %d\n", coder -> id, dongle_id);

  pthread_mutex_lock(&data->state_mutex);
  request.deadline = coder->last_compile + data->burnout;
  pthread_mutex_unlock(&data->state_mutex);

  // push the request
  pthread_mutex_lock(&d->mutex);
  heap_push(&d->queue, request);
  // countinue
  while (!is_stopped(data)) {
    // check if its not taken and aslo if that request is top request
    if (!(d->taken) && (top_request(&d->queue) == coder->id)) {
      // check cooldown peroid
      if (get_time_ms() >= d->available_at) {

        //printf("taken coder %d to dongle %d\n", coder->id, dongle_id);
        d->taken = 1;
        // take request out of the dongle s heap
        heap_pop_first(&d->queue);
        pthread_mutex_unlock(&d->mutex);
        return (1);
      }
      // if still not avaible ba9i sekhouna
      ts.tv_sec = d->available_at / 1000;
      ts.tv_nsec = (d->available_at % 1000) * 1000000;
      pthread_cond_timedwait(&d->cond, &d->mutex, &ts);

    } else
      // if its taken
      // wait on that cond variable until somth change for that dongle
      pthread_cond_wait(&d->cond, &d->mutex);
  }
  // if the sumilation stopped
  remove_request(&d->queue, coder->id);
  pthread_mutex_unlock(&d->mutex);
  return (0);
}

int take_dongles(t_coder *coder) {
  int first;
  int second;
  int tmp;

  first = coder->left;
  second = coder->right;

  // put just in order here we have resource hierarchy 
  // this prevent the deadlockl
  if (first > second) {
    tmp = first;
    first = second;
    second = tmp;
  }

  if (!take_dongle(coder, first))
    return (0);

  // if there is only one dongle one coder and already i took one
  //  so just wait till summulation and give up
  if (first == second) {
    print_status(coder, "has taken a dongle");
    while (!is_stopped(coder->data))
      sleep_for_ms(1, coder->data);

    // the sum is realase the dongle
    // relase first dongle
    realease_dongles(coder);
    return (0);
  }

  if (!take_dongle(coder, second)) {
    // release first dongle
    realease_dongles(coder);
    return (0);
  }
  // print both
  print_status(coder, "has taken a dongle");
  print_status(coder, "has taken a dongle");

  return (1);
}

void realease_dongles(t_coder *coder) {
  int i;
  int id;
  t_data *data;

  data = coder->data;
  i = 0;
  while (i < 2) {
    if (i == 0)
      id = coder->left;
    else
      id = coder->right;

    // if we have only one dongle we took only one should we already relase only
    // one
    if (i == 1 && coder->left == coder->right)
      break;

    pthread_mutex_lock(&data->dongles[id].mutex);
    if (data->dongles[id].taken) {
      data->dongles[id].taken = 0;
      data->dongles[id].available_at = get_time_ms() + data->cooldown;
      // now when its free broadcast to all other waiting threads with that cond
      // variable
      pthread_cond_broadcast(&data->dongles[id].cond);
    }
    pthread_mutex_unlock(&data->dongles[id].mutex);
    i++;
  }
}
