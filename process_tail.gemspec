# coding: utf-8
lib = File.expand_path('../lib', __FILE__)
$LOAD_PATH.unshift(lib) unless $LOAD_PATH.include?(lib)
require 'process_tail/version'

Gem::Specification.new do |spec|
  spec.name          = 'process_tail'
  spec.version       = ProcessTail::VERSION
  spec.authors       = ['Hika Hibariya']
  spec.email         = ['hibariya@gmail.com']
  spec.summary       = %q{Get other process outputs.}
  spec.homepage      = 'https://github.com/hibariya/process_tail'
  spec.license       = 'MIT'

  spec.files         = `git ls-files -z`.split("\x0")
  spec.executables   = spec.files.grep(%r{^bin/}) { |f| File.basename(f) }
  spec.test_files    = spec.files.grep(%r{^(test|spec|features)/})
  spec.require_paths = ['lib']

  spec.extensions = %w(ext/process_tail/extconf.rb)

  spec.add_development_dependency 'bundler',       '~> 1.7'
  spec.add_development_dependency 'rake',          '~> 10.0'
  spec.add_development_dependency 'rake-compiler', '~> 0.9.5'
  spec.add_development_dependency 'rspec',         '~> 3.1.0'
end
