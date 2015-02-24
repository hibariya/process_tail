#include <sys/types.h>
#include <ruby.h>
#include "extconf.h"
#include "process_tail.h"
#include "pt_tracee.h"
#ifdef HAVE_SYS_PTRACE_H
#include "pt_ptrace.h"
#endif
#ifdef HAVE_DTRACE_H
#include "pt_dtrace.h"
#endif

static void
pt_process_tail_mark(pt_process_tail_t *ptp)
{
  rb_gc_mark(ptp->proc);
  rb_gc_mark(ptp->trace_thread);
  rb_gc_mark(ptp->wait_queue);
}

static VALUE
pt_process_tail_alloc(VALUE klass)
{
  pt_process_tail_t *ptp = ALLOC(pt_process_tail_t);

  return Data_Wrap_Struct(klass, pt_process_tail_mark, RUBY_DEFAULT_FREE, ptp);
}

static VALUE
pt_process_tail_initialize(VALUE self, VALUE pidv, VALUE fdv)
{
  pt_process_tail_t *ptp;

  if (!rb_block_given_p()) {
    rb_raise(rb_eArgError, "no block given");
  }

  Check_Type(pidv, T_FIXNUM);
  Check_Type(fdv, T_FIXNUM);

  Data_Get_Struct(self, pt_process_tail_t, ptp);

  ptp->pid           = (pid_t)NUM2INT(pidv);
  ptp->fd            = (unsigned int)NUM2INT(fdv);
  ptp->proc          = rb_block_proc();
  ptp->trace_thread  = (VALUE)NULL;
  ptp->parent_thread = (VALUE)NULL;
  ptp->wait_queue    = (VALUE)NULL;
  ptp->tracee        = NULL;

  return Qnil;
}

static VALUE
pt_process_tail_pid_reader(VALUE self)
{
  pt_process_tail_t *ptp;

  Data_Get_Struct(self, pt_process_tail_t, ptp);

  return INT2NUM((int)ptp->pid);
}

static VALUE
pt_process_tail_fd_reader(VALUE self)
{
  pt_process_tail_t *ptp;

  Data_Get_Struct(self, pt_process_tail_t, ptp);

  return INT2NUM((int)ptp->fd);
}

static VALUE
pt_process_tail_trace_thread_reader(VALUE self)
{
  pt_process_tail_t *ptp;

  Data_Get_Struct(self, pt_process_tail_t, ptp);

  return ptp->trace_thread;
}

VALUE ProcessTail;
VALUE ProcessTail_Tracer;
VALUE ProcessTail_StopTracing;
VALUE ProcessTail_TraceError;

void
pt_lock_trace(void)
{
  rb_funcall(PT_TRACE_LOCK, rb_intern("lock"), 0);
}

void
pt_unlock_trace(void)
{
  rb_funcall(PT_TRACE_LOCK, rb_intern("unlock"), 0);
}

void
Init_process_tail(void)
{
  ProcessTail = rb_define_module("ProcessTail");

  ProcessTail_StopTracing = rb_define_class_under(ProcessTail, "StopTracing", rb_eStandardError);
  ProcessTail_TraceError  = rb_define_class_under(ProcessTail, "TraceError",  rb_eStandardError);

  ProcessTail_Tracer = rb_define_class_under(ProcessTail, "Tracer", rb_cObject);
  rb_define_const(ProcessTail_Tracer, "TRACE_LOCK", rb_class_new_instance(0, NULL, rb_path2class("Mutex")));

  rb_define_alloc_func(ProcessTail_Tracer, pt_process_tail_alloc);
  rb_define_private_method(ProcessTail_Tracer, "initialize", pt_process_tail_initialize, 2);

  rb_define_method(ProcessTail_Tracer, "pid",          pt_process_tail_pid_reader,          0);
  rb_define_method(ProcessTail_Tracer, "fd",           pt_process_tail_fd_reader,           0);
  rb_define_method(ProcessTail_Tracer, "trace_thread", pt_process_tail_trace_thread_reader, 0);

#ifdef HAVE_SYS_PTRACE_H
  rb_define_private_method(ProcessTail_Tracer, "ptrace_attach",  pt_ptrace_attach, 0);
#endif

#ifdef HAVE_DTRACE_H
  rb_define_private_method(ProcessTail_Tracer, "dtrace_attach",  pt_dtrace_attach, 0);
#endif
}
