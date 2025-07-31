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
  while (q->running) {
    MQueueMessage *m = mqueue_get_in(q);
    if (m == NULL) {
      continue;
    }

    int16_t a = (int)m->message & 0xFFFF;
    int16_t b = (((int)m->message) >> 16) & 0xFFFF;
    int result = gcd(a, b);
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
  do {
    uint32_t data = 0;
    int16_t a;
    int16_t b;
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

    int my_result = gcd(a, b);
    for (int i = 0; i < RLOOP; i++) {
      if (request[i] == -1) {
        continue;
      }
#if 0
      struct timespec sleep = {0, 10};
      thrd_sleep(&sleep, NULL);
#endif
      int x = (int)mqueue_get_out(q, request[i]);
      if (x == (uintptr_t)NULL) {
        continue;
      }
    }
  } while (q->running);
  fclose(fp);
  return 0;
}

int main(int argc, char **argv) {
  MQueue *queue = mqueue_create();
  assert(queue != NULL && "HOLY SHIET");
  pthread_t r[RTHD];
  pthread_t w[WTHD];

  for (int i = 0; i < WTHD; i++) {
    pthread_create(&w[i], NULL, worker_thread, queue);
  }

  for (int i = 0; i < RTHD; i++) {
    pthread_create(&r[i], NULL, request_thread, queue);
  }

  sleep(10);

  mqueue_destroy(queue, NULL);
  for (int i = 0; i < WTHD; i++) {
    pthread_join(w[i], NULL);
  }
  for (int i = 0; i < RTHD; i++) {
    pthread_join(r[i], NULL);
  }
}
