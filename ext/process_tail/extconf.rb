require 'mkmf'

%w(string.h sys/ptrace.h sys/syscall.h sys/types.h sys/user.h sys/wait.h ruby.h ruby/thread.h).each do |h|
  abort "missing #{h}" unless have_header(h)
end

abort 'missing ptrace' unless have_func('ptrace', 'sys/ptrace.h')

%w(orig_rax rdi rdx rsi rdx).each do |m|
  abort "missing #{m}" unless have_struct_member('struct user_regs_struct', m, 'sys/user.h')
end

create_makefile 'process_tail/process_tail'
