# ProcessTail

Get other process outputs.

## Installation

Add this line to your application's Gemfile:

```ruby
gem 'process_tail'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install process_tail

## Usage

```ruby
io, th = ProcessTail.trace(pid, :stdout)

puts "Output of #{pid}: #{io.gets}"
```

## Contributing

1. Fork it ( https://github.com/hibariya/process_tail/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
