#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <ruby.h>
#include <ruby/thread.h>

struct
process_tail_args {
  pid_t pid;
  int *status;
  unsigned int fd;
  VALUE io;
};

static void *
process_tail_waitpid(void *argp)
{
  struct process_tail_args *args = argp;

  waitpid(args->pid, args->status, WUNTRACED | WCONTINUED);

  return NULL;
}

static int
process_tail_attach(void *argp)
{
  struct process_tail_args *args = argp;

  if (ptrace(PTRACE_ATTACH, args->pid, NULL, NULL) < 0) {
    rb_sys_fail("PTRACE_ATTACH");
    return -1;
  }

  rb_thread_call_without_gvl(process_tail_waitpid, (void *)argp, RUBY_UBF_PROCESS, NULL);

  return 0;
}

static VALUE
process_tail_detach(VALUE argp)
{
  struct process_tail_args *args = (void *)argp;

  ptrace(PTRACE_DETACH, args->pid, NULL, NULL);

  return Qnil;
}

static void
process_tail_get_data(pid_t pid, long addr, char *string, int len)
{
  long data;
  int  i;

  for (i = 0; i < len; i += sizeof(long)) {
    data = ptrace(PTRACE_PEEKDATA, pid, addr + i);

    memcpy(string + i, &data, sizeof(long));
  }

  string[len] = '\0';
}

static VALUE
process_tail_loop(VALUE argp)
{
  struct process_tail_args *args = (void *)argp;
  struct user_regs_struct regs;
  int in_syscall = 0;
  int signal     = 0;
  int status;
  char *string   = NULL;

  args->status = &status;

  while (ptrace(PTRACE_SYSCALL, args->pid, NULL, signal) == 0) {
    rb_thread_call_without_gvl(process_tail_waitpid, args, RUBY_UBF_PROCESS, NULL);

    signal = 0;

    if (WIFEXITED(status)) {
      return Qnil;
    }

    if (!WIFSTOPPED(status)) {
      continue;
    }

    switch (WSTOPSIG(status)) {
      case SIGTRAP:
        ptrace(PTRACE_GETREGS, args->pid, 0, &regs);

        if (regs.orig_rax != SYS_write) {
          continue;
        }

        in_syscall = 1 - in_syscall;
        if (in_syscall && (args->fd == 0 || regs.rdi == args->fd)) {
          string = malloc(regs.rdx);
          process_tail_get_data(args->pid, regs.rsi, string, regs.rdx);

          if (rb_funcall(args->io, rb_intern("closed?"), 0) == Qtrue) {
            return Qnil;
          }

          rb_io_write(args->io, rb_str_new_cstr(string));

          free(string);
        }
      case SIGVTALRM:
        break;
      case SIGSTOP:
        break;
      default:
        signal = WSTOPSIG(status);
    }

    rb_thread_schedule();
  }

  return Qnil;
}

static VALUE
process_tail_trace(VALUE klass, VALUE pidp, VALUE fdp, VALUE io)
{
  pid_t pid       = (pid_t)FIX2INT(pidp);
  unsigned int fd = (unsigned int)FIX2INT(fdp);

  struct process_tail_args args = {pid, NULL, fd, io};

  Check_Type(pidp, T_FIXNUM);
  Check_Type(fdp, T_FIXNUM);
  Check_Type(io, T_FILE);

  if (process_tail_attach((void *)&args) == 0) {
    rb_ensure(process_tail_loop, (VALUE)&args, process_tail_detach, (VALUE)&args);
  }

  return Qnil;
}

void
Init_process_tail()
{
  VALUE ProcessTail;

  ProcessTail = rb_define_module("ProcessTail");
  rb_define_module_function(ProcessTail, "do_trace", process_tail_trace, 3);
}
