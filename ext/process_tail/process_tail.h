#ifndef PROCESS_TAIL_H
#define PROCESS_TAIL_H
#include <sys/types.h>
#include <ruby.h>
#include "extconf.h"
#include "pt_tracee.h"

#define PT_TRACE_LOCK (rb_const_get(ProcessTail_Tracer, rb_intern("TRACE_LOCK")))

extern VALUE ProcessTail;
extern VALUE ProcessTail_Tracer;
extern VALUE ProcessTail_StopTracing;

typedef struct {
  pid_t pid;
  unsigned int fd;
  VALUE proc;
  VALUE trace_thread;
  VALUE parent_thread;
  VALUE wait_queue;
  pt_tracee_t *tracee;
  void *data;
} pt_process_tail_t;

void pt_lock_trace(void);
void pt_unlock_trace(void);

void Init_process_tail(void);

#endif
