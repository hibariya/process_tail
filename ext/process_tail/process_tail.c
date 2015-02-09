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

typedef struct pt_wait_args {
  int *status;
  pid_t pid;
} pt_wait_args_t;

static pt_tracee_t *
pt_tracee_add(pt_tracee_t **headpp, pid_t pid)
{
  pt_tracee_t *tracee = malloc(sizeof(pt_tracee_t));

  if (tracee == NULL) {
    exit(EXIT_FAILURE);
  }

  tracee->pid       = pid;
  tracee->activated = 0;
  tracee->dead      = 0;
  tracee->next      = (struct pt_tracee *)*headpp;

  (*headpp) = tracee;

  return tracee;
}

static pt_tracee_t *
pt_tracee_find_or_add(pt_tracee_t **headpp, pid_t pid)
{
  pt_tracee_t *tracee = *headpp;

  while (tracee != NULL) {
    if (tracee->pid == pid) {
      return tracee;
    }

    tracee = tracee->next;
  }

  return pt_tracee_add(headpp, pid);
}

static int
pt_tracee_wipedoutq(pt_tracee_t *headp)
{
  pt_tracee_t *tracee = headp;

  while (tracee != NULL) {
    if (tracee->dead == 0) {
      return 0;
    }

    tracee = tracee->next;
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

static int
pt_io_closedq(VALUE io)
{
  return rb_funcall(io, rb_intern("closed?"), 0) == Qtrue;
}

static void *
pt_wait(void *argp)
{
  pt_wait_args_t *args = (pt_wait_args_t *)argp;

  args->pid = waitpid(-1, args->status, __WALL);

  return NULL;
}

static void
pt_get_data(pid_t pid, long addr, char *string, int length)
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

static void
pt_loop(unsigned int fd, VALUE write_io, VALUE read_io, VALUE wait_queue, pt_tracee_t *tracee_headp)
{
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
    tracee = pt_tracee_find_or_add(&tracee_headp, pid);

    if (pt_io_closedq(read_io)) {
      pt_tracee_free(&tracee_headp);

      return;
    }

    if (tracee->activated == 0) {
      ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK | PTRACE_O_TRACECLONE);
      tracee->activated = 1;

      rb_funcall(wait_queue, rb_intern("enq"), 1, INT2FIX((int)pid));
      pt_ptrace_syscall(pid, signal);

      continue;
    }

    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      tracee->dead = 1;

      if (pt_tracee_wipedoutq(tracee_headp)) {
        pt_tracee_free(&tracee_headp);

        return;
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
    if (syscall && (fd == 0 || regs.rdi == fd)) {
      string = malloc(regs.rdx + sizeof(long));

      if (string == NULL) {
        exit(EXIT_FAILURE);
      }

      pt_get_data(pid, regs.rsi, string, regs.rdx);

      if (pt_io_closedq(read_io)) {
        free(string);
        pt_tracee_free(&tracee_headp);

        return;
      }

      rb_io_write(write_io, rb_str_new_cstr(string));

      free(string);
    }

    pt_ptrace_syscall(pid, signal);
  }
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
pt_trace(VALUE klass, VALUE fdp, VALUE write_io, VALUE read_io, VALUE wait_queue)
{
  pt_tracee_t *tracee_headp = NULL;

  Check_Type(fdp,         T_FIXNUM);
  Check_Type(write_io,    T_FILE);
  Check_Type(read_io,     T_FILE);

  pt_loop((unsigned int)FIX2INT(fdp), write_io, read_io, wait_queue, tracee_headp);

  return Qnil;
}

void
Init_process_tail()
{
  VALUE ProcessTail = rb_define_module("ProcessTail");

  rb_define_singleton_method(ProcessTail, "attach",   pt_attach, 1);
  rb_define_singleton_method(ProcessTail, "do_trace", pt_trace,  4);
  rb_define_singleton_method(ProcessTail, "detach",   pt_detach, 1);
}
