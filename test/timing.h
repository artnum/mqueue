#ifndef TIMING_H__
#define TIMING_H__

void *timing_start();
void timing_stop(void *_timing, void *_t);
void timing_print(void *_t);
void timing_destroy(void *_t);
void *timing_new();

#endif /* TIMING_H__ */
