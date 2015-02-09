require 'thread'
require 'process_tail/process_tail'
require 'process_tail/version'

module ProcessTail
  TRACE_LOCK = Mutex.new # NOTE For now, ProcessTail can't trace multiple processes at the same time

  class << self
    def open(pid, fd = :stdout)
      read_io, write_io = IO.pipe
      trace_thread      = trace(pid, fd) {|str, *|
        unless read_io.closed?
          write_io.write str
        else
          trace_thread.kill
        end
      }

      wait_thread = fork_wait_thread(trace_thread, [read_io, write_io])

      if block_given?
        value = yield(read_io)

        trace_thread.kill
        wait_thread.join

        value
      else
        read_io
      end
    end

    def trace(pid, fd = :stdout, &block)
      TRACE_LOCK.synchronize {
        trace_without_lock(pid, fd, &block)
      }
    end

    private

    def trace_without_lock(pid, fd, &block)
      task_ids     = extract_task_ids(pid)
      wait_queue   = Queue.new
      trace_thread = Thread.fork {
        begin
          task_ids.each do |tid|
            attach tid
          end

          do_trace extract_fd(fd), wait_queue, &block
        ensure
          task_ids.each do |tid|
            detach tid
          end
        end
      }

      wait_all_attach task_ids, wait_queue

      trace_thread
    end

    def fork_wait_thread(trace_thread, pipe)
      Thread.fork {
        begin
          trace_thread.join
        ensure
          pipe.each do |io|
            io.close unless io.closed?
          end
        end
      }
    end

    def wait_all_attach(task_ids, waitq)
      task_ids.size.times do
        waitq.deq
      end
    end

    def extract_task_ids(pid)
      Dir.open("/proc/#{pid}/task") {|dir|
        dir.entries.grep(/^\d+$/).map(&:to_i)
      }
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
