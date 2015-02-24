#ifndef PT_TRACEE_H
#define PT_TRACEE_H

#include <sys/types.h>

typedef struct pt_tracee {
  pid_t pid;
  int activated;
  int dead;
  struct pt_tracee *next;
} pt_tracee_t;

void pt_tracee_initialize(pt_tracee_t *tracee);
pt_tracee_t *pt_tracee_add(pt_tracee_t **headpp, pid_t pid);
pt_tracee_t *pt_tracee_find_or_add(pt_tracee_t **headpp, pid_t pid);
int pt_tracee_wipedoutq(pt_tracee_t *headp);
void pt_tracee_free(pt_tracee_t **headpp);
int pt_tracee_length(pt_tracee_t *tracee);

#endif
