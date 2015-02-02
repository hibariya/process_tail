require 'process_tail'

describe ProcessTail do
  def process_maker(&fn)
     -> {
      Process.fork {
        trap :USR1 do
          sleep 0.1 # XXX :(

          fn.call
        end

        sleep
      }
    }
  end

  def expect_same_output_and_no_error(fd, string)
    greeting = process_maker { puts string }
    pid      = greeting.call
    io, th   = ProcessTail.trace(pid, :stdout)

    Process.kill :USR1, pid # resume child

    "#{string}\n".each_line do |expected|
      expect(io.gets).to eq expected
    end

    Process.kill :TERM, pid

    expect { th.join }.to_not raise_error
  end

  describe '.trace' do
    it do
      expect_same_output_and_no_error :stdout, 'HELLO'
      expect_same_output_and_no_error :stderr, 'HELLO'
      expect_same_output_and_no_error :stdout, "HELLO\nLOVE AND KISSES"
    end
  end
end
