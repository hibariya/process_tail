#include <stdio.h>
#include <dtrace.h>
#include <ruby.h>
#include <ruby/thread.h>
#include "process_tail.h"
#include "pt_dtrace.h"

static int
probe_consumer(const dtrace_probedata_t *data, void *argp)
{
  pt_process_tail_t *pt;

  dtrace_eprobedesc_t *edesc = data->dtpda_edesc;
  dtrace_recdesc_t *rec;
  caddr_t addr;

  int i;
  VALUE arg;
  VALUE args = rb_ary_new_capa(3);

  Data_Get_Struct((VALUE)argp, pt_process_tail_t, pt);

  for (i = 0; i < edesc->dtepd_nrecs; i++) {
    arg = 0;
    rec =  &edesc->dtepd_rec[i];

    if (rec->dtrd_size > 0) {
      addr = data->dtpda_data + rec->dtrd_offset;

      switch(rec->dtrd_action) {
      case DTRACEACT_DIFEXPR:
        switch(rec->dtrd_size) {
          case 1:
            arg = INT2NUM((int)(*((uint8_t *)addr)));
            break;
          case 2:
            arg = INT2NUM((int)(*((uint16_t *)addr)));
            break;
          case 4:
            arg = INT2NUM(*((int32_t *)addr));
            break;
          case 8:
            arg = LL2NUM(*((int64_t *)addr));
            break;
          default:
            arg = rb_str_new_cstr((char *)addr);
        }

        rb_ary_push(args, arg);
        break;
      default:
        break;
      }
    }
  }

  rb_proc_call(pt->proc, args);

  return (DTRACE_CONSUME_THIS);
}

static void *
pt_dtrace_sleep(void *argp)
{
  dtrace_hdl_t *hdl = (dtrace_hdl_t *)argp;

  dtrace_sleep(hdl);

  return NULL;
}

static VALUE
pt_loop(VALUE self)
{
  pt_process_tail_t *pt;
  dtrace_hdl_t *hdl;

  VALUE dscriptv = rb_funcall(self, rb_intern("generate_dscript"), 0);
  dtrace_prog_t *program;
  dtrace_proginfo_t *proginfo;

  FILE *devnull = fopen("/dev/null", "w");
  int activated = 0;
  int done      = 0;

  Data_Get_Struct(self, pt_process_tail_t, pt);

  hdl = PT_DTRACE_HDL(pt);

  dtrace_setopt(hdl, "stacksymbols", "enabled");
  dtrace_setopt(hdl, "bufsize", "4m");
  dtrace_setopt(hdl, "quiet", 0);

  program = dtrace_program_strcompile(hdl, StringValueCStr(dscriptv), DTRACE_PROBESPEC_NAME, DTRACE_C_PSPEC, 0, NULL);

  if (!program) {
    rb_raise(ProcessTail_TraceError, "dtrace_program_strcompile: %s", dtrace_errmsg(hdl, dtrace_errno(hdl)));
    return Qnil;
  }

  if (dtrace_program_exec(hdl, program, proginfo) < 0) {
    rb_raise(ProcessTail_TraceError, "dtrace_program_exec: %s", dtrace_errmsg(hdl, dtrace_errno(hdl)));
    return Qnil;
  }

  if (dtrace_go(hdl) < 0) {
    rb_raise(ProcessTail_TraceError, "dtrace_go: %s", dtrace_errmsg(hdl, dtrace_errno(hdl)));
    return Qnil;
  }

  do {
    if (!done) {
      if (!activated) {
        rb_funcall(pt->wait_queue, rb_intern("enq"), 1, INT2NUM((int)pt->pid));

        activated = 1;
      }

      rb_thread_call_without_gvl(pt_dtrace_sleep, hdl, RUBY_UBF_PROCESS, NULL);
    }

    if (done) {
      if(dtrace_stop(hdl) == -1) {
        rb_raise(ProcessTail_TraceError, "dtrace_stop: %s", dtrace_errmsg(hdl, dtrace_errno(hdl)));
        return Qnil;
      }
    }

    switch (dtrace_work(hdl, devnull, probe_consumer, NULL, (void *)self)) {
    case DTRACE_WORKSTATUS_DONE:
      done = 1;
      break;
    case DTRACE_WORKSTATUS_OKAY:
      break;
    default:
      break;
    }

    rb_thread_schedule();
  } while(!done);

  return Qnil;
}

static VALUE
pt_finalize(VALUE self)
{
  pt_process_tail_t *pt;

  Data_Get_Struct(self, pt_process_tail_t, pt);

  dtrace_close(PT_DTRACE_HDL(pt));

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

VALUE
pt_dtrace_attach(VALUE self)
{
  pt_process_tail_t *pt;

  int error;
  dtrace_hdl_t *hdl = dtrace_open(DTRACE_VERSION, 0, &error);

  Data_Get_Struct(self, pt_process_tail_t, pt);

  if (!hdl) {
    rb_raise(ProcessTail_TraceError, "dtrace_open: %s", strerror(error));
    return Qnil;
  }

  pt->tracee        = NULL; // NOT USED
  pt->data          = (void *)hdl;
  pt->wait_queue    = rb_class_new_instance(0, NULL, rb_path2class("Queue"));
  pt->parent_thread = rb_thread_current();
  pt->trace_thread  = rb_thread_create(pt_loop_thread, (void *)self);

  rb_funcall(pt->wait_queue, rb_intern("deq"), 0);

  return pt->trace_thread;
}
