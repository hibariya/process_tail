require 'process_tail/process_tail'
require 'process_tail/version'
require 'erb'
require 'thread'

module ProcessTail
  module ReadIOExtention
    attr_accessor :process_tail_after_close

    def close
      super
    ensure
      if hook = process_tail_after_close
        hook.call
      end
    end
  end

  class Tracer
    def attach
      case
      when respond_to?(:ptrace_attach, true)
        ptrace_attach
      when respond_to?(:dtrace_attach, true)
        dtrace_attach
      else
        raise NotImplementedError
      end

      self
    end

    def detach
      trace_thread.kill.join

      self
    end

    private

    def generate_dscript
      template = File.read(File.join(__dir__, 'process_tail/process_tail.d'))

      ERB.new(template).result(binding)
    end
  end

  class << self
    def open(pid, fd = :stdout)
      read_io, write_io = IO.pipe

      tracer = trace(pid, fd) {|tid, fd_num, str|
        unless read_io.closed?
          write_io.write str
        else
          write_io.close
          process_tail.detach
        end
      }

      read_io.extend ReadIOExtention
      read_io.process_tail_after_close = -> {
        write_io.close unless write_io.closed?
        tracer.detach
      }

      block_given? ? yield(read_io) : read_io
    ensure
      if block_given?
        [read_io, write_io].each do |io|
          io.close unless io.closed?
        end
      end
    end

    private

    def trace(pid, fd, &block)
      pt = Tracer.new(pid, extract_fd(fd), &block)
      pt.attach

      pt
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
