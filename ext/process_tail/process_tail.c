#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <ruby.h>
#include <ruby/thread.h>

typedef struct pt_tracee {
  pid_t pid;
  int activated;
  int dead;
  struct pt_tracee *next;
} pt_tracee_t;

typedef struct {
  int *status;
  pid_t pid;
} pt_wait_args_t;

typedef struct {
  unsigned int fd;
  pt_tracee_t *tracee;
  VALUE *wait_queue;
} pt_loop_args_t;

static void
pt_tracee_initialize(pt_tracee_t *tracee)
{
  tracee->pid       = 0;
  tracee->activated = 0;
  tracee->dead      = 0;
  tracee->next      = NULL;
}

static pt_tracee_t *
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

static pt_tracee_t *
pt_tracee_find_or_add(pt_tracee_t **headpp, pid_t pid)
{
  pt_tracee_t *headp   = *headpp;
  pt_tracee_t *current = headp;

  while (current->next) {
    if (current->pid == pid) {
      return current;
    }

    current = current->next;
  }

  return pt_tracee_add(headpp, pid);
}

static int
pt_tracee_wipedoutq(pt_tracee_t *headp)
{
  pt_tracee_t *current = headp;

  while (current->next) {
    if (current->dead == 0) {
      return 0;
    }

    current = current->next;
  }

  return 1;
}

static void
pt_tracee_free(pt_tracee_t **headpp)
{
  pt_tracee_t *tracee = *headpp;

  if (tracee->next) {
    pt_tracee_free(&tracee->next);
  }

  free(tracee);
  tracee = NULL;
}

static void *
pt_wait(void *argp)
{
  pt_wait_args_t *args = (pt_wait_args_t *)argp;

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
  ptrace(PTRACE_SYSCALL, pid, 0, signal);

  rb_thread_schedule();
}

static VALUE
pt_loop(VALUE loop_args)
{
  pt_loop_args_t *args = (void *)loop_args;

  char *string = NULL;
  int syscall  = 0;
  int status;
  long signal;
  pid_t pid;
  pt_tracee_t *tracee;
  pt_wait_args_t pt_wait_args = {&status};

  // NOTE: system call: eax, arguments: rdi, rsi, rdx, r10, r8, r9
  struct user_regs_struct regs;

  while (1) {
    rb_thread_call_without_gvl(pt_wait, &pt_wait_args, RUBY_UBF_PROCESS, NULL);

    signal = 0;
    pid    = pt_wait_args.pid;
    tracee = pt_tracee_find_or_add(&args->tracee, pid);

    if (tracee->activated == 0) {
      tracee->activated = 1;
      rb_funcall(*args->wait_queue, rb_intern("enq"), 1, INT2FIX((int)pid));

      ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE);
      pt_ptrace_syscall(pid, signal);

      continue;
    }

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      tracee->dead = 1;

      if (pt_tracee_wipedoutq(args->tracee)) {
        return Qnil;
      }

      continue;
    }

    if (!(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80)) {
      signal = WSTOPSIG(status);
      pt_ptrace_syscall(pid, signal);

      continue;
    }

    ptrace(PTRACE_GETREGS, pid, 0, &regs);

    if (regs.orig_rax != SYS_write) {
      pt_ptrace_syscall(pid, signal);

      continue;
    }

    syscall = 1 - syscall;
    if (syscall && (args->fd == 0 || regs.rdi == args->fd)) {
      string = malloc(regs.rdx + sizeof(long));

      if (string == NULL) {
        exit(EXIT_FAILURE);
      }

      pt_read_data(pid, regs.rsi, string, regs.rdx);

      rb_yield(rb_ary_new_from_args(3, rb_str_new_cstr(string), INT2FIX((int)pid), INT2FIX(args->fd)));

      free(string);
    }

    pt_ptrace_syscall(pid, signal);
  }

  return Qnil;
}

static VALUE
pt_attach(VALUE klass, VALUE pidv)
{
  int pid = (pid_t)FIX2INT(pidv);

  Check_Type(pidv, T_FIXNUM);

  if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
    rb_sys_fail("PTRACE_ATTACH");

    return Qnil;
  }

  return Qtrue;
}

static VALUE
pt_detach(VALUE klass, VALUE pidv)
{
  Check_Type(pidv, T_FIXNUM);

  ptrace(PTRACE_DETACH, (pid_t)FIX2INT(pidv), NULL, NULL);

  return Qnil;
}

static VALUE
pt_finalize(VALUE traceev)
{
  pt_tracee_t *tracee = (void *)traceev;

  if (tracee->next) {
    pt_tracee_free(&tracee);
  }

  return Qnil;
}

static VALUE
pt_trace(VALUE klass, VALUE fdv, VALUE wait_queue)
{
  pt_tracee_t  tracee;
  pt_loop_args_t loop_args = {FIX2INT(fdv), &tracee, &wait_queue};

  Check_Type(fdv, T_FIXNUM);

  pt_tracee_initialize(&tracee);

  rb_ensure(pt_loop, (VALUE)&loop_args, pt_finalize, (VALUE)&tracee);

  return Qnil;
}

void
Init_process_tail()
{
  VALUE ProcessTail = rb_define_module("ProcessTail");

  // TODO: private
  rb_define_singleton_method(ProcessTail, "attach",   pt_attach, 1);
  rb_define_singleton_method(ProcessTail, "do_trace", pt_trace,  2);
  rb_define_singleton_method(ProcessTail, "detach",   pt_detach, 1);
}
