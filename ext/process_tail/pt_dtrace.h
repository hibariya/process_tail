#ifndef PT_DTRACE_H
#define PT_DTRACE_H

#include <ruby.h>

#define PT_DTRACE_HDL(pt) ((dtrace_hdl_t *)pt->data)

VALUE pt_dtrace_attach(VALUE self);

#endif
