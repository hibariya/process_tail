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
process_tail_waitpid_args {
  pid_t pid;
  int *status;
};

static void *
process_tail_waitpid(void *argp)
{
  struct process_tail_waitpid_args *args = argp;

  waitpid(args->pid, args->status, WUNTRACED | WCONTINUED);

  return NULL;
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

static void
process_tail_loop(pid_t pid, unsigned int fd, VALUE write_io, VALUE read_io)
{
  int in_syscall = 0;
  int signal     = 0;
  int status;
  char *string   = NULL;
  struct process_tail_waitpid_args wait_args = {pid, &status};
  struct user_regs_struct regs;

  while (ptrace(PTRACE_SYSCALL, pid, NULL, signal) == 0) {
    rb_thread_call_without_gvl(process_tail_waitpid, &wait_args, RUBY_UBF_PROCESS, NULL);

    signal = 0;

    if (WIFEXITED(status)) {
      return;
    }

    if (!WIFSTOPPED(status)) {
      continue;
    }

    switch (WSTOPSIG(status)) {
      case SIGTRAP:
        ptrace(PTRACE_GETREGS, pid, 0, &regs);

        if (regs.orig_rax != SYS_write) {
          continue;
        }

        in_syscall = 1 - in_syscall;
        if (in_syscall && (fd == 0 || regs.rdi == fd)) {
          string = malloc(regs.rdx + sizeof(long));
          process_tail_get_data(pid, regs.rsi, string, regs.rdx);

          if (rb_funcall(read_io, rb_intern("closed?"), 0) == Qtrue) {
            return;
          }

          rb_io_write(write_io, rb_str_new_cstr(string));

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
}

static VALUE
process_tail_attach(VALUE klass, VALUE pidp)
{
  int status;
  int pid = (pid_t)FIX2INT(pidp);
  struct process_tail_waitpid_args wait_args = {pid, &status};

  Check_Type(pidp, T_FIXNUM);

  if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
    rb_sys_fail("PTRACE_ATTACH");

    return Qnil;
  }

  rb_thread_call_without_gvl(process_tail_waitpid, &wait_args, RUBY_UBF_PROCESS, NULL);

  return Qtrue;
}

static VALUE
process_tail_detach(VALUE klass, VALUE pidp)
{
  Check_Type(pidp, T_FIXNUM);

  ptrace(PTRACE_DETACH, (pid_t)FIX2INT(pidp), NULL, NULL);

  return Qnil;
}

static VALUE
process_tail_trace(VALUE klass, VALUE pidp, VALUE fdp, VALUE write_io, VALUE read_io)
{
  Check_Type(pidp,     T_FIXNUM);
  Check_Type(fdp,      T_FIXNUM);
  Check_Type(write_io, T_FILE);
  Check_Type(read_io,  T_FILE);

  process_tail_loop((pid_t)FIX2INT(pidp), (unsigned int)FIX2INT(fdp), write_io, read_io);

  return Qnil;
}

void
Init_process_tail()
{
  VALUE ProcessTail = rb_define_module("ProcessTail");

  rb_define_singleton_method(ProcessTail, "ptrace_attach", process_tail_attach, 1);
  rb_define_singleton_method(ProcessTail, "do_trace",      process_tail_trace,  4);
  rb_define_singleton_method(ProcessTail, "ptrace_detach", process_tail_detach, 1);
}
