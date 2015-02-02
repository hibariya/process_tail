require 'mkmf'

abort 'missing ptrace' unless have_func('ptrace')

create_makefile 'process_tail/process_tail'
