require 'process_tail'

describe ProcessTail do
  def process_maker(&fn)
     -> {
      Process.fork {
        trap :USR1 do
          sleep 0.1 # XXX :(

          fn.call if fn
        end

        sleep
      }
    }
  end

  def expect_same_output_and_no_error(fd, method, string)
    greeting = process_maker { send(method, string) }
    pid      = greeting.call
    io, th   = ProcessTail.trace(pid, fd)

    Process.kill :USR1, pid # resume child

    "#{string}\n".each_line do |expected|
      expect(io.gets).to eq expected
    end

    Process.kill :TERM, pid

    expect { th.join }.to_not raise_error
  end

  describe '.open' do
    specify 'without block' do
      pid = process_maker.call

      read_io = ProcessTail.open(pid, :stdout)

      expect(read_io).to_not be_closed

      read_io.close
      Process.kill :TERM, pid
    end

    specify 'with block' do
      pid     = process_maker.call
      read_io = nil

      ProcessTail.open pid, :stdout  do |io|
        read_io = io

        expect(read_io).to_not be_closed
      end

      expect(read_io).to be_closed

      Process.kill :TERM, pid
    end
  end

  describe '.trace' do
    it do
      expect_same_output_and_no_error :stdout, :puts, 'HELLO'
      expect_same_output_and_no_error :stderr, :warn, 'HELLO'
      expect_same_output_and_no_error :stdout, :puts, "HELLO\nLOVE AND KISSES"
    end
  end
end
