require 'process_tail/process_tail'
require 'process_tail/version'

module ProcessTail
  module_function

  def trace(pid, fd = :stdout)
    read_io, write_io = IO.pipe

    thread = Thread.fork {
      begin
        do_trace pid, extract_fd(fd), write_io
      ensure
        [read_io, write_io].each do |io|
          io.close unless io.closed?
        end
      end
    }

    [read_io, thread]
  end

  def extract_fd(name)
    case name
    when :stdout
      1
    when :stderr
      2
    when :all
      0
    else
      Integer(name)
    end
  end
end
