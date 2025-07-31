#ifndef MQUEUE_H__
#define MQUEUE_H__

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct s_MQueueMessage MQueueMessage;
struct s_MQueueMessage {
  int id;
  uintptr_t message;
  MQueueMessage *next;
  MQueueMessage *previous;
};

#ifndef MQUEUE_HASH_MAP_SIZE
#define MQUEUE_HASH_MAP_SIZE 256 // 29 // 7829
#endif

#define MQUEUE_LOCK_STATS
#ifdef MQUEUE_LOCK_STATS
struct MQueueMutexStats {
  pthread_mutex_t mstat;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int max;
  int min;
  float avg;
  long int count;
  int *values;
};
#endif

typedef struct {
  _Atomic int next_message_id;
  _Atomic int waiters;
  _Atomic bool running; /* allow to stop the queue from running */

  struct {
    MQueueMessage *head;
    MQueueMessage *tail;
  } in;

  MQueueMessage *out[MQUEUE_HASH_MAP_SIZE];
  pthread_mutex_t out_mutexes[MQUEUE_HASH_MAP_SIZE];
  pthread_cond_t out_conds[MQUEUE_HASH_MAP_SIZE];

  pthread_mutex_t in_mutex;
  pthread_cond_t in_cond;
} MQueue;

bool mqueue_add_out(MQueue *q, MQueueMessage *m);
uintptr_t mqueue_get_out(MQueue *q, int id);
MQueueMessage *mqueue_get_in(MQueue *q);
int mqueue_add_in(MQueue *q, uintptr_t msg);
void mqueue_destroy(MQueue *q, void (*free_message)(uintptr_t message));
MQueue *mqueue_create(void);
#endif /* MQUEUE_H__ */
