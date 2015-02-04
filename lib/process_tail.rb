require 'thread'
require 'process_tail/process_tail'
require 'process_tail/version'

module ProcessTail
  class << self
    def open(pid, fd = :stdout)
      io = trace(pid, fd)

      block_given? ? yield(io) : io
    ensure
      io.close if block_given? && !io.closed?
    end

    def trace(pid, fd = :stdout)
      read_io, write_io = IO.pipe
      task_ids   = extract_task_ids(pid)
      wait_queue = Queue.new

      thread = Thread.fork {
        begin
          extract_task_ids(pid).each do |tid|
            attach tid
          end

          do_trace extract_fd(fd), write_io, read_io, wait_queue
        ensure
          write_io.close unless write_io.closed?

          task_ids.each do |tid|
            detach tid
          end
        end
      }

      at_exit do
        thread.kill
        thread.join
      end

      wait_all_attach task_ids, wait_queue

      read_io
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
