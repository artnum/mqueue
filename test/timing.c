#include <limits.h>
#include <pthread.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct Times {
  struct timespec start;
  struct timespec stop;
  long int id;
  long int diff;
  int *next;
};
struct Timing {
  long int id_count;
  pthread_mutex_t mutex;
  struct Times *head;
};

void *timing_new() {
  struct Timing *t = NULL;
  t = calloc(1, sizeof(*t));
  if (t) {
    pthread_mutex_init(&t->mutex, NULL);
    t->head = NULL;
  }
  return t;
}

void timing_destroy(void *_t) {
  struct Timing *timing = _t;
  if (timing == NULL) {
    return;
  }
  for (struct Times *t = timing->head; t;) {
    struct Times *n = (struct Times *)t->next;
    free(t);
    t = n;
  }
  free(timing);
}

void *timing_start() {
  struct Times *t = calloc(1, sizeof(struct Times));
  if (t) {
    clock_gettime(CLOCK_MONOTONIC, &t->start);
  }
  return t;
}

void timing_stop(void *_timing, void *_t) {
  struct Times *t = _t;
  struct Timing *timing = _timing;
  if (t) {
    clock_gettime(CLOCK_MONOTONIC, &t->stop);
    long int ns_start = t->start.tv_nsec + (t->start.tv_sec * 1000000000);
    long int ns_stop = t->stop.tv_nsec + (t->stop.tv_sec * 1000000000);
    t->diff = ns_stop - ns_start;
    pthread_mutex_lock(&timing->mutex);
    t->next = (void *)timing->head;
    timing->head = t;
    pthread_mutex_unlock(&timing->mutex);
  }
}
#define GSIZE_X 69
#define GSIZE_Y 20
#define GCHAR_PEAK '#'
#define GCHAR_FILLED '*'
#define GCHAR_EMPTY ' '
#define GCHAR_NONE '.'
void timing_print(void *_t) {
  struct Timing *timing = _t;
  pthread_mutex_lock(&timing->mutex);
  int graph[GSIZE_X] = {0};
  long int max_in_graph[GSIZE_X] = {0};

  int count = 0;
  long int accumulated = 0;
  long int max = 0;
  long int min = INT_MAX;
  for (struct Times *t = timing->head; t;) {
    struct Times *n = (struct Times *)t->next;
    long int diff = t->diff;

    if (diff > max) {
      max = diff;
    }
    if (diff < min) {
      min = diff;
    }
    accumulated += diff;
    count++;

    t = n;
  }
  float avg = (float)accumulated / (float)count;

  int count_90max = 0;
  int count_80max = 0;
  int count_70max = 0;
  int count_60max = 0;
  int count_50max = 0;
  int count_40max = 0;
  int count_30max = 0;
  int count_20max = 0;
  int count_10max = 0;
  int count_below_avg = 0;
  for (struct Times *t = timing->head; t; t = (struct Times *)t->next) {
    long int diff = t->diff;

    graph[(diff - min) * GSIZE_X / (max - min + 1)]++;
    if (max_in_graph[(diff - min) * GSIZE_X / (max - min + 1)] < diff) {
      max_in_graph[(diff - min) * GSIZE_X / (max - min + 1)] = diff;
    }

    if ((float)diff >= (float)max * 0.9) {
      count_90max++;
    }
    if ((float)diff >= (float)max * 0.8 && (float)diff < (float)max * 0.9) {
      count_80max++;
    }
    if ((float)diff >= (float)max * 0.7 && (float)diff < (float)max * 0.8) {
      count_70max++;
    }
    if ((float)diff >= (float)max * 0.6 && (float)diff < (float)max * 0.7) {
      count_60max++;
    }
    if ((float)diff >= (float)max * 0.5 && (float)diff < (float)max * 0.6) {
      count_50max++;
    }
    if ((float)diff >= (float)max * 0.4 && (float)diff < (float)max * 0.5) {
      count_40max++;
    }
    if ((float)diff >= (float)max * 0.3 && (float)diff < (float)max * 0.4) {
      count_30max++;
    }
    if ((float)diff >= (float)max * 0.2 && (float)diff < (float)max * 0.3) {
      count_20max++;
    }
    if ((float)diff >= (float)max * 0.1 && (float)diff < (float)max * 0.2) {
      count_10max++;
    }
    if ((float)diff < avg) {
      count_below_avg++;
    }
  }

  pthread_mutex_unlock(&timing->mutex);

  printf("--- TIMINGS ---\n");
  printf("- Round-trip coun     \t% 9d\n", count);
  printf("- Average [ns]        \t% 12.2f\n", avg);
  printf("- Max [ns]            \t% 9ld\n", max);
  printf("- Min [ns]            \t% 9ld\n", min);
  printf("- >90%% of max        \t% 9d\n", count_90max);
  printf("- >80%% of max        \t% 9d\n", count_80max);
  printf("- >70%% of max        \t% 9d\n", count_70max);
  printf("- >60%% of max        \t% 9d\n", count_60max);
  printf("- >50%% of max        \t% 9d\n", count_50max);
  printf("- >40%% of max        \t% 9d\n", count_40max);
  printf("- >30%% of max        \t% 9d\n", count_30max);
  printf("- >20%% of max        \t% 9d\n", count_20max);
  printf("- >10%% of max        \t% 9d\n", count_10max);
  printf("- Below average       \t% 9d\t %12.2f %%\n", count_below_avg,
         (float)count_below_avg * 100 / (float)count);

  printf("\n");

  char char_graph[GSIZE_X][GSIZE_Y] = {' '};
  for (int i = 0; i < GSIZE_X; i++) {
    for (int j = 0; j <= graph[i] % GSIZE_Y; j++) {
      char_graph[i][j] = graph[i] == 0 ? GCHAR_NONE : GCHAR_FILLED;
    }
    for (int j = graph[i] % GSIZE_Y + 1; j < GSIZE_Y; j++) {
      char_graph[i][j] = GCHAR_EMPTY;
    }
  }
  const char *colors[2] = {"\e[38;5;8m", "\e[38;5;9m"};
  int c = 0;
  for (int i = GSIZE_Y - 1; i >= 0; i--) {
    int c = 0;
    for (int j = 0; j < GSIZE_X; j++) {
      if (char_graph[j][i] == GCHAR_FILLED && i + 1 < GSIZE_Y &&
          char_graph[j][i + 1] != GCHAR_FILLED) {
        c = 1;
      }
      if (i + 1 >= GSIZE_Y && char_graph[j][i] == GCHAR_FILLED) {
        c = 1;
      }
      printf("%s%c", colors[c], c == 1 ? GCHAR_PEAK : char_graph[j][i]);
      c = 0;
    }
    printf("\n");
  }
#if 0
  for (int i = 0; i < GSIZE_X; i++) {
    if (max_in_graph[i] > 0) {
      printf("% 2d: % 8ld\n", i, max_in_graph[i]);
    } else {

      printf("\n");
    }
  }
#endif
}
