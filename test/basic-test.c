#include "../src/include/mqueue.h"
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

_Atomic int request_count;
_Atomic int response_count;
_Atomic int request_failed;
_Atomic int response_failed;
_Atomic int run;
_Atomic int no_more_request;

int gcd(int a, int b) {
  if (b == 0) {
    return a;
  }
  return gcd(a, a % b);
}

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

    int16_t a = (int)m->message & 0xFFFF;
    int16_t b = (((int)m->message) >> 16) & 0xFFFF;
    int result = gcd(a, b);
    if (result == 0) {
      result = 1;
    }
    m->message = (uintptr_t)result;
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
  struct timespec start = {0};
  struct timespec stop = {0};
  int thid = thrd_current();
  FILE *fp = fopen("/dev/random", "r");
  if (fp == NULL) {
    return NULL;
  }
  while (atomic_load(&run) == 1) {
    uint32_t data = 0;
    int16_t a;
    int16_t b;
    if (atomic_load(&no_more_request) == 0) {
      for (int i = 0; i < RLOOP; i++) {
        fread(&data, sizeof(uint32_t), 1, fp);
        a = data & 0xFFFF;
        b = (data >> 16) & 0xFFFF;
        request[i] = mqueue_add_in(q, data);
        if (request[i] == -1) {
          break;
        }
        x[i] = data;
      }
    }

    for (int i = 0; i < RLOOP; i++) {
      if (request[i] == -1) {
        continue;
      }
      int x = (int)mqueue_get_out(q, request[i]);
      if (x == (uintptr_t)NULL) {
        atomic_fetch_add(&response_failed, 1);
        continue;
      }
      atomic_fetch_add(&response_count, 1);
    }
  }
  fclose(fp);
  return 0;
}

int main(int argc, char **argv) {
  MQueue *queue = mqueue_create();
  assert(queue != NULL && "Failed create queue");
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
  sleep(10);

  /* stop request let finish response */
  atomic_store(&no_more_request, 1);
  sleep(2);
  atomic_store(&run, 0);

  printf("--- ----\n");
  printf("REQUESTS         :\t% 10d\n", atomic_load(&request_count));
  printf("RESPONSES        :\t% 10d\n", atomic_load(&response_count));
  printf("REQUESTS failed  :\t% 10d\n", atomic_load(&request_failed));
  printf("RESPONSES failed :\t% 10d\n", atomic_load(&response_failed));

  mqueue_destroy(queue, NULL);
  for (int i = 0; i < WTHD; i++) {
    pthread_join(w[i], NULL);
  }
  for (int i = 0; i < RTHD; i++) {
    pthread_join(r[i], NULL);
  }
}
