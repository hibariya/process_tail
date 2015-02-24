#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <ruby.h>
#include <ruby/thread.h>
#include "process_tail.h"
#include "pt_tracee.h"
#include "pt_ptrace.h"

typedef struct {
  int *status;
  pid_t pid;
} pt_wait_args_t;

static pt_tracee_t *
pt_extract_tracee_list(pid_t pid)
{
  pt_tracee_t  *tracee = NULL;
  char procdir[sizeof("/proc/%d/task") + sizeof(int) * 3];
  DIR *dir;
  struct dirent *entry;
  pid_t tid;

  sprintf(procdir, "/proc/%d/task", pid);
  dir = opendir(procdir);

  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_fileno == 0) {
      continue;
    }

    tid = (pid_t)atoi(entry->d_name);

    if (tid <= 0) {
      continue;
    }

    tracee = pt_tracee_find_or_add(&tracee, tid);
  }

  closedir(dir);

  return tracee;
}

static void *
pt_wait(void *argv)
{
  pt_wait_args_t *args = (pt_wait_args_t *)argv;

  args->pid = waitpid(-1, args->status, __WALL);

  return NULL;
}

static void
pt_read_data(pid_t pid, long addr, char *string, int length)
{
  int  i;
  long data;

  for (i = 0; i < length; i += sizeof(long)) {
    data = ptrace(PTRACE_PEEKDATA, pid, addr + i);

    memcpy(string + i, &data, sizeof(long));
  }

  string[length] = '\0';
}

static void
pt_ptrace_syscall(pid_t pid, long signal)
{
  ptrace(PTRACE_SYSCALL, pid, 0, signal == SIGTRAP || signal == SIGSTOP ? 0 : signal);

  rb_thread_schedule();
}

static int
pt_ptrace_attach_multi(pt_tracee_t *tracee)
{
  do {
    if (ptrace(PTRACE_ATTACH, tracee->pid, NULL, NULL) < 0) {
      return 0;
    }
  } while ((tracee = tracee->next) != NULL);

  return 1;
}

static VALUE
pt_loop(VALUE self)
{
  pt_process_tail_t *pt;

  pt_tracee_t *tracee;

  pid_t pid;
  int status;
  pt_wait_args_t wait_args = {&status};

  int syscall  = 0;
  char *string = NULL;

  struct user_regs_struct regs; // NOTE: system call: orig_rax, arguments: rdi, rsi, rdx, r10, r8, r9

  Data_Get_Struct(self, pt_process_tail_t, pt);

  if (!pt_ptrace_attach_multi(pt->tracee)) {
    rb_raise(ProcessTail_TraceError, "PTRACE_ATTACH");

    return Qnil;
  }

  while (1) {
    rb_thread_call_without_gvl(pt_wait, &wait_args, RUBY_UBF_PROCESS, NULL);

    pid    = wait_args.pid;
    tracee = pt_tracee_find_or_add(&pt->tracee, pid);

    if (tracee->activated == 0) {
      tracee->activated = 1;
      rb_funcall(pt->wait_queue, rb_intern("enq"), 1, INT2NUM((int)pid));

      ptrace(PTRACE_SETOPTIONS, pid, 0,
        PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE
      );

      pt_ptrace_syscall(pid, 0);

      continue;
    }

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      tracee->dead = 1;

      if (pt_tracee_wipedoutq(pt->tracee)) {
        return Qnil;
      }

      continue;
    }

    if (!(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80)) {
      pt_ptrace_syscall(pid, WSTOPSIG(status));

      continue;
    }

    ptrace(PTRACE_GETREGS, pid, 0, &regs);

    if (regs.orig_rax != SYS_write) {
      pt_ptrace_syscall(pid, 0);

      continue;
    }

    syscall = 1 - syscall;
    if (syscall && (pt->fd == 0 || regs.rdi == pt->fd)) {
      string = malloc(regs.rdx + sizeof(long));

      if (string == NULL) {
        exit(EXIT_FAILURE);
      }

      pt_read_data(pid, regs.rsi, string, regs.rdx);

      rb_proc_call(pt->proc, rb_ary_new_from_args(3, INT2NUM((int)pid), INT2NUM(regs.rdi), rb_str_new_cstr(string)));

      free(string);
    }

    pt_ptrace_syscall(pid, 0);
  }

  return Qnil;
}

static VALUE
pt_finalize(VALUE self)
{
  pt_process_tail_t *pt;
  Data_Get_Struct(self, pt_process_tail_t, pt);

  ptrace(PTRACE_DETACH, pt->pid, NULL, NULL);
  pt_tracee_free(&pt->tracee);

  pt_unlock_trace();
  rb_funcall(pt->parent_thread, rb_intern("raise"), 1, ProcessTail_StopTracing);

  return Qnil;
}

static VALUE
pt_loop_thread(void *self)
{
  pt_lock_trace();

  rb_ensure(pt_loop, (VALUE)self, pt_finalize, (VALUE)self);

  return Qnil;
}

static void
wait_queue_ntimes(VALUE wait_queue, int n)
{
  int i;

  for (i = 0; i < n; i++) {
    rb_funcall(wait_queue, rb_intern("deq"), 0);
  }
}

VALUE
pt_ptrace_attach(VALUE self)
{
  pt_process_tail_t *pt;

  Data_Get_Struct(self, pt_process_tail_t, pt);

  pt->tracee        = pt_extract_tracee_list(pt->pid);
  pt->wait_queue    = rb_class_new_instance(0, NULL, rb_path2class("Queue"));
  pt->parent_thread = rb_thread_current();
  pt->trace_thread  = rb_thread_create(pt_loop_thread, (void *)self);

  wait_queue_ntimes(pt->wait_queue, pt_tracee_length(pt->tracee));

  return pt->trace_thread;
}
