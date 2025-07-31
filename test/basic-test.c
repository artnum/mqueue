#include "../src/include/mqueue.h"
#include "timing.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#define WTHD 2
#define RTHD 6
#define RLOOP 1

void *timing = NULL;

_Atomic int request_count;
_Atomic int response_count;
_Atomic int request_failed;
_Atomic int response_failed;
_Atomic int run;
_Atomic int no_more_request;

void *worker_thread(void *queue) {
  MQueue *q = queue;
  int max = 0;
  int min = INT_MAX;
  struct timespec start = {0};
  struct timespec stop = {0};
  while (atomic_load(&run) == 1) {
    MQueueMessage *m = mqueue_get_in(q);
    if (m == NULL) {
      atomic_fetch_add(&request_failed, 1);
      continue;
    }
    atomic_fetch_add(&request_count, 1);
    mqueue_add_out(q, m);
  }

  return 0;
}

void *request_thread(void *queue) {
  MQueue *q = queue;
  int request[RLOOP] = {0};
  uint32_t x[RLOOP] = {0};

  int max = 0;
  int min = INT_MAX;
  int thid = thrd_current();
  while (atomic_load(&run) == 1) {
    if (atomic_load(&no_more_request) == 0) {
      for (int i = 0; i < RLOOP; i++) {
        void *t = timing_start();
        request[i] = mqueue_add_in(q, (uintptr_t)t);
        if (request[i] == -1) {
          break;
        }
      }
    }

    for (int i = 0; i < RLOOP; i++) {
      if (request[i] == -1) {
        continue;
      }
      uintptr_t x = mqueue_get_out(q, request[i]);
      if (x == (uintptr_t)NULL) {
        atomic_fetch_add(&response_failed, 1);
        continue;
      }
      timing_stop((void *)timing, (void *)x);
      atomic_fetch_add(&response_count, 1);
    }
  }
  return 0;
}

int main(int argc, char **argv) {
  MQueue *queue = mqueue_create();
  assert(queue != NULL && "Failed create queue");

  timing = timing_new();
  assert(timing != NULL && "Failed to create timing");

  pthread_t r[RTHD];
  pthread_t w[WTHD];
  atomic_store(&run, 1);
  atomic_store(&no_more_request, 0);
  for (int i = 0; i < WTHD; i++) {
    pthread_create(&w[i], NULL, worker_thread, queue);
  }

  for (int i = 0; i < RTHD; i++) {
    pthread_create(&r[i], NULL, request_thread, queue);
  }

  /* let run for some time */
  sleep(60);

  /* stop request let finish response */
  atomic_store(&no_more_request, 1);
  sleep(2);
  atomic_store(&run, 0);

  printf("--- REQUESTS and RESPONSES---\n");
  printf("REQUESTS         :\t% 10d\n", atomic_load(&request_count));
  printf("RESPONSES        :\t% 10d\n", atomic_load(&response_count));
  printf("REQUESTS failed  :\t% 10d\n", atomic_load(&request_failed));
  printf("RESPONSES failed :\t% 10d\n", atomic_load(&response_failed));
  printf("\n");
  timing_print(timing);

  mqueue_destroy(queue, NULL);
  timing_destroy(timing);
  for (int i = 0; i < WTHD; i++) {
    pthread_join(w[i], NULL);
  }
  for (int i = 0; i < RTHD; i++) {
    pthread_join(r[i], NULL);
  }
}
