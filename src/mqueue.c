#include "include/mqueue.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#define IS_QUEUE_RUNNING(q) atomic_load(&(q)->running)
#define SET_QUEUE_RUNNING(q, r) atomic_store(&(q)->running, r)
#define GET_NEXT_ID(q) atomic_fetch_add(&(q)->next_message_id, 1);
#define ADD_WAITER(q) atomic_fetch_add(&(q)->waiters, 1)
#define RM_WAITER(q) atomic_fetch_sub(&(q)->waiters, 1)

const struct timespec cond_wait_time = {0, 500};

MQueue *mqueue_create(void) {
  MQueue *q = NULL;
  q = calloc(1, sizeof(MQueue));

  if (q != NULL) {
    pthread_mutex_init(&q->in_mutex, NULL);
    for (int i = 0; i < MQUEUE_HASH_MAP_SIZE; i++) {
      pthread_mutex_init(&q->out_mutexes[i], NULL);
      pthread_cond_init(&q->out_conds[i], NULL);
    }
    pthread_cond_init(&q->in_cond, NULL);
    /* queue is running on create */
    SET_QUEUE_RUNNING(q, true);
    atomic_store(&q->waiters, 0);
  }
  return q;
}

void mqueue_destroy(MQueue *q, void (*free_message)(uintptr_t message)) {
  assert(q);
  SET_QUEUE_RUNNING(q, false);
  /* trigger all waiting thread to wakeup, as q->running is false, they will
   * stop waitaing
   * It's a busy waiting loop.
   */
  while (atomic_load(&q->waiters) > 0)
    ;

  pthread_mutex_lock(&q->in_mutex);
  MQueueMessage *m = q->in.head;
  while (m != NULL) {
    if (free_message) {
      free_message(m->message);
    }
    MQueueMessage *n = m->next;
    free(m);
    m = n;
  }
  q->in.tail = NULL;
  q->in.head = NULL;
  for (int i = 0; i < MQUEUE_HASH_MAP_SIZE; i++) {
    if (q->out[i] != NULL) {
      pthread_mutex_lock(&q->out_mutexes[i]);
      m = q->out[i];
      while (m) {
        MQueueMessage *n = m->next;
        if (free_message) {
          free_message(m->message);
        }
        free(m);
        m = n;
      }
      q->out[i] = NULL;
      pthread_mutex_unlock(&q->out_mutexes[i]);
    }
  }
  pthread_mutex_unlock(&q->in_mutex);
  pthread_mutex_destroy(&q->in_mutex);
  pthread_cond_destroy(&q->in_cond);

  for (int i = 0; i < MQUEUE_HASH_MAP_SIZE; i++) {
    pthread_mutex_destroy(&q->out_mutexes[i]);
    pthread_cond_destroy(&q->out_conds[i]);
  }
  free(q);
}

int mqueue_add_in(MQueue *q, uintptr_t msg) {
  assert(q != NULL);
  assert(msg != (uintptr_t)NULL);
  int msg_id = -1;

  MQueueMessage *m = calloc(1, sizeof(*m));
  if (m != NULL) {
    msg_id = GET_NEXT_ID(q);
    m->id = msg_id;
    m->message = msg;
    m->next = NULL;
    m->previous = NULL;

    pthread_mutex_lock(&q->in_mutex);
    if (!IS_QUEUE_RUNNING(q)) {
      free(m);
      pthread_mutex_unlock(&q->in_mutex);
      return NULL;
    }
    m->next = q->in.head;
    if (q->in.head == NULL) {
      q->in.tail = m;
    } else {
      q->in.head->previous = m;
    }
    q->in.head = m;
    pthread_cond_broadcast(&q->in_cond);
    pthread_mutex_unlock(&q->in_mutex);
  }

  return msg_id;
}

MQueueMessage *mqueue_get_in(MQueue *q) {
  assert(q != NULL);
  MQueueMessage *m = NULL;
  MQueueMessage *p = NULL;

  pthread_mutex_lock(&q->in_mutex);
  ADD_WAITER(q);
  while (q->in.tail == NULL) {
    if (!IS_QUEUE_RUNNING(q)) {
      pthread_mutex_unlock(&q->in_mutex);
      RM_WAITER(q);
      return NULL;
    }
    pthread_cond_timedwait(&q->in_cond, &q->in_mutex, &cond_wait_time);
  }
  m = q->in.tail;
  q->in.tail = m->previous;
  if (q->in.tail == NULL) {
    q->in.head = NULL;
  }
  pthread_mutex_unlock(&q->in_mutex);
  RM_WAITER(q);
  m->previous = NULL;
  m->next = NULL;

  return m;
}

uintptr_t mqueue_get_out(MQueue *q, int id) {
  assert(q != NULL);

  MQueueMessage *m = NULL;

  int idx = id % MQUEUE_HASH_MAP_SIZE;
  ADD_WAITER(q);
  while (m == NULL) {
    pthread_mutex_lock(&q->out_mutexes[idx]);
    while (q->out[idx] == NULL) {
      if (!IS_QUEUE_RUNNING(q)) {
        pthread_mutex_unlock(&q->out_mutexes[idx]);
        RM_WAITER(q);
        return NULL;
      }
      pthread_cond_timedwait(&q->out_conds[idx], &q->out_mutexes[idx],
                             &cond_wait_time);
    }

    MQueueMessage *p = NULL;
    m = q->out[idx];
    while (m && m->id != id) {
      m = m->next;
    }

    if (m) {
      if (m == q->out[idx]) {
        q->out[idx] = m->next;
      } else {
        if (m->next) {
          m->next->previous = m->previous;
        }
        if (m->previous) {
          m->previous->next = m->next;
        }
      }
    }
    pthread_mutex_unlock(&q->out_mutexes[idx]);
  }
  RM_WAITER(q);
  uintptr_t v = (uintptr_t)NULL;
  if (m) {
    v = m->message;
    free(m);
  }
  return v;
}

bool mqueue_add_out(MQueue *q, MQueueMessage *m) {
  assert(q != NULL);
  assert(m != NULL);

  m->next = NULL;
  m->previous = NULL;
  int idx = m->id % MQUEUE_HASH_MAP_SIZE;

  pthread_mutex_lock(&q->out_mutexes[idx]);
  if (!IS_QUEUE_RUNNING(q)) {
    pthread_mutex_unlock(&q->out_mutexes[idx]);
    return false;
  }
  if (q->out[idx] == NULL) {
    q->out[idx] = m;
  } else {
    m->next = q->out[idx];
    q->out[idx]->previous = m;
    q->out[idx] = m;
  }
  pthread_cond_broadcast(&q->out_conds[idx]);
  pthread_mutex_unlock(&q->out_mutexes[idx]);

  return true;
}
