require 'bundler/gem_tasks'
require 'rspec/core/rake_task'
require 'rake/extensiontask'

Rake::ExtensionTask.new 'process_tail' do |ext|
  ext.lib_dir = 'lib/process_tail'
end

RSpec::Core::RakeTask.new(:spec)

task default: [:compile, :spec]
