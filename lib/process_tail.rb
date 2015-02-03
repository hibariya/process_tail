require 'process_tail/process_tail'
require 'process_tail/version'

module ProcessTail
  class << self
    def open(pid, fd = :stdout)
      read_io, thread = trace(pid, fd)

      block_given? ? yield(read_io) : read_io
    ensure
      finalize = -> {
        read_io.close unless read_io.closed?
        thread.kill
      }

      block_given? ? finalize.call : at_exit(&finalize)
    end

    def trace(pid, fd = :stdout)
      read_io, write_io = IO.pipe

      thread = Thread.fork {
        begin
          do_trace pid, extract_fd(fd), write_io
        ensure
          begin
            [read_io, write_io].each do |io|
              io.close unless io.closed?
            end
          rescue IOError; end
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
end
