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

    ProcessTail.open pid, fd do |io|
      Process.kill :USR1, pid # resume child

      "#{string}\n".each_line do |expected|
        expect(io.gets).to eq expected
      end
    end
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

      sleep 1 # FIXME :(
    end
  end

  describe '.open' do
    specify 'without block' do
      pid = process_maker.call

      read_io = ProcessTail.open(pid, :stdout)

      expect(read_io).to_not be_closed

      expect(read_io.close).to be_nil
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

    specify 'simple stdout'  do
      expect_same_output_and_no_error :stdout, :puts, 'HELLO'
    end

    specify 'simple stderr' do
      expect_same_output_and_no_error :stderr, :warn, 'HELLO'
    end

    specify 'multiline' do
      expect_same_output_and_no_error :stdout, :puts, "HELLO\nHELLO"
    end

    specify 'multithreaded' do
      pid = Process.fork {
        trap :USR1 do
          puts 'HELLO'
        end

        Thread.fork do
          trap :USR2 do
            puts 'LOVE AND KISSES'
          end

          sleep
        end

        sleep
      }

      $recorded_children << pid
      io = ProcessTail.open(pid, :stdout)

      Process.kill :USR1, pid
      Process.kill :USR2, pid

      expect(io.gets).to eq "HELLO\n"
      expect(io.gets).to eq "LOVE AND KISSES\n"

      io.close
    end
  end
end
