require 'mkmf'

%w(string.h sys/ptrace.h sys/syscall.h sys/types.h sys/user.h sys/wait.h ruby.h ruby/thread.h).each do |h|
  abort "missing #{h}" unless have_header(h)
end

%w(orig_rax rdi rdx rsi rdx).each do |m|
  abort "missing #{m}" unless have_struct_member('struct user_regs_struct', m, 'sys/user.h')
end

%w(PTRACE_O_TRACESYSGOOD PTRACE_O_TRACEFORK PTRACE_O_TRACEVFORK PTRACE_O_TRACECLONE).each do |c|
  abort "missing #{c}" unless have_const(c, 'sys/ptrace.h')
end

abort 'missing __WALL' unless have_const('__WALL', 'sys/wait.h')

create_makefile 'process_tail/process_tail'
