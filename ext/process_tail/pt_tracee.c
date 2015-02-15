#include <sys/types.h>
#include <stdlib.h>
#include "pt_tracee.h"

void
pt_tracee_initialize(pt_tracee_t *tracee)
{
  tracee->pid       = 0;
  tracee->activated = 0;
  tracee->dead      = 0;
  tracee->next      = NULL;
}

pt_tracee_t *
pt_tracee_add(pt_tracee_t **headpp, pid_t pid)
{
  pt_tracee_t *tracee = malloc(sizeof(pt_tracee_t));

  if (tracee == NULL) {
    exit(EXIT_FAILURE);
  }

  pt_tracee_initialize(tracee);
  tracee->pid  = pid;
  tracee->next = (struct pt_tracee *)*headpp;

  (*headpp) = tracee;

  return tracee;
}

pt_tracee_t *
pt_tracee_find_or_add(pt_tracee_t **headpp, pid_t pid)
{
  pt_tracee_t *headp   = *headpp;
  pt_tracee_t *current = headp;

  while (current) {
    if (current->pid == pid) {
      return current;
    }

    current = current->next;
  }

  return pt_tracee_add(headpp, pid);
}

int
pt_tracee_wipedoutq(pt_tracee_t *headp)
{
  pt_tracee_t *current = headp;

  while (current) {
    if (current->dead == 0) {
      return 0;
    }

    current = current->next;
  }

  return 1;
}

void
pt_tracee_free(pt_tracee_t **headpp)
{
  pt_tracee_t *tracee = *headpp;

  if (tracee) {
    pt_tracee_free(&tracee->next);

    free(tracee);
    tracee = NULL;
  }
}

int
pt_tracee_length(pt_tracee_t *tracee)
{
  pt_tracee_t *current;
  int length = 0;

  if (tracee == NULL) {
    return length;
  }

  current = tracee;

  do {
    length++;
  } while ((current = current->next) != NULL);

  return length;
}
