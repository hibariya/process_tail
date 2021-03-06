require 'mkmf'

objs = []

-> {
  return unless have_library('dtrace', 'dtrace_open', 'dtrace.h')

  have_header 'dtrace.h'

  objs << 'pt_dtrace.o'
}.()

-> {
  return unless %w(orig_rax rdi rdx rsi rdx).all? {|m|
    have_struct_member('struct user_regs_struct', m, 'sys/user.h')
  }

  return unless %w(sys/syscall.h sys/types.h sys/user.h sys/wait.h).all? {|h|
    have_header(h)
  }

  return unless have_const('__WALL', 'sys/wait.h')

  return unless %w(PTRACE_O_TRACESYSGOOD PTRACE_O_TRACEFORK PTRACE_O_TRACEVFORK PTRACE_O_TRACECLONE).all? {|c|
    have_const(c, 'sys/ptrace.h')
  }

  have_header 'sys/ptrace.h'

  objs << 'pt_ptrace.o'
}.()

abort 'No available libraries' if objs.empty?

$objs = %w(process_tail.o pt_tracee.o) + objs

create_header
create_makefile 'process_tail/process_tail'
