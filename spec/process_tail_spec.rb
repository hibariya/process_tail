require 'process_tail'
require 'timeout'

describe ProcessTail do
  def process_maker(&fn)
     -> {
      pid = Process.fork {
        trap :USR1 do
          fn.call if fn
        end

        sleep
      }

      $recorded_children << pid

      pid
    }
  end

  def expect_same_output_and_no_error(fd, method, string)
    greeting    = process_maker { send(method, string) }
    pid         = greeting.call
    io          = ProcessTail.trace(pid, fd)

    Process.kill :USR1, pid # resume child

    "#{string}\n".each_line do |expected|
      expect(io.gets).to eq expected
    end

    io.close
  end

  around do |example|
    begin
      $recorded_children = []

      # at least every examples should be finished within 5 seconds
      timeout 5 do
        example.run
      end

    ensure
      $recorded_children.each do |child|
        Process.kill :TERM, child
      end
    end
  end

  describe '.open' do
    specify 'without block' do
      pid = process_maker.call

      read_io = ProcessTail.open(pid, :stdout)

      expect(read_io).to_not be_closed

      read_io.close
    end

    specify 'with block' do
      pid     = process_maker.call
      read_io = nil

      ProcessTail.open pid, :stdout do |io|
        read_io = io

        expect(read_io).to_not be_closed
      end

      expect(read_io).to be_closed
    end
  end

  describe '.trace' do
    specify 'simple stdout'  do
      expect_same_output_and_no_error :stdout, :puts, 'HELLO'
    end

    specify 'simple stderr' do
      expect_same_output_and_no_error :stderr, :warn, 'HELLO'
    end

    specify 'multithreaded' do
      pid = Process.fork {
        trap :USR1 do
          puts 'HELLO'
        end

        Thread.fork do
          trap :USR2 do
            puts 'HELLO'
          end

          sleep
        end

        sleep
      }

      sleep 1
      $recorded_children << pid
      io = ProcessTail.trace(pid, :stdout)

      Process.kill :USR1, pid
      Process.kill :USR2, pid

      expect(io.gets).to eq "HELLO\n"
      expect(io.gets).to eq "HELLO\n"

      io.close
    end

    specify 'multiline' do
      expect_same_output_and_no_error :stdout, :puts, "HELLO\nLOVE AND KISSES"
    end
  end
end
