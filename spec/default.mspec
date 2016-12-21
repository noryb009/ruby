# -*- ruby -*-
load "./rbconfig.rb"
load File.dirname(__FILE__) + '/rubyspec/default.mspec'
OBJDIR = File.expand_path("spec/rubyspec/optional/capi/ext")
class MSpecScript
  builddir = Dir.pwd
  srcdir = ENV['SRCDIR']
  if !srcdir and File.exist?("#{builddir}/Makefile") then
    File.open("#{builddir}/Makefile", "r:US-ASCII") {|f|
      f.read[/^\s*srcdir\s*=\s*(.+)/i] and srcdir = $1
    }
  end
  srcdir = File.expand_path(srcdir)
  config = RbConfig::CONFIG

  # The default implementation to run the specs.
  set :target, File.join(builddir, "miniruby#{config['exeext']}")
  set :prefix, File.expand_path('rubyspec', File.dirname(__FILE__))
  set :flags, %W[
    -I#{srcdir}/lib
    -I#{srcdir}
    -I#{srcdir}/#{config['EXTOUT']}/common
    #{srcdir}/tool/runruby.rb --archdir=#{Dir.pwd} --extout=#{config['EXTOUT']}
    --
  ]

  # These fail from an upstream Ruby issue.
  set :xpatterns, [
    'Float#round returns big values rounded to nearest',
    'Integer#round returns itself rounded to nearest if passed a negative value',
    'Kernel#clone returns nil for NilClass',
    'Kernel#clone returns true for TrueClass',
    'Kernel#clone returns false for FalseClass',
    'Kernel#clone returns the same Integer for Integer',
    'Kernel#clone returns the same Symbol for Symbol',
    'Kernel#dup returns nil for NilClass',
    'Kernel#dup returns true for TrueClass',
    'Kernel#dup returns false for FalseClass',
    'Kernel#dup returns the same Integer for Integer',
    'Kernel#dup returns the same Symbol for Symbol',
    'Rational#round with no arguments (precision = 0) returns the rounded value toward the nearest integer',
    'Time#strftime rounds an offset to the nearest second when formatting with %z',
    'BigDecimal#inspect looks like this',
    'BigDecimal.new raises ArgumentError for invalid strings'
  ]
end
