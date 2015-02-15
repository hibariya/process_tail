# ProcessTail [![Build Status](https://travis-ci.org/hibariya/process_tail.svg?branch=master)](https://travis-ci.org/hibariya/process_tail)

https://github.com/hibariya/process_tail

## Description

ProcessTail traces other process' write(2) system call and copy its buffer.
So you can get other process outputs.

## Problems

* Windows is not supported at the moment
* SEGV occures occasionally

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

### Get outputs as an IO object

```ruby
ProcessTail.open pid, :stdout do |io|
  puts "Recent stdout of #{pid}: #{io.gets}"
end
```

### Show outputs instantly

```bash
$ process_tail $(pgrep PROCESS_NAME)
```

## Contributing

1. Fork it ( https://github.com/hibariya/process_tail/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
