require 'test/unit'
require "-test-/string/string"

class Test_StringModifyExpand < Test::Unit::TestCase
  def test_modify_expand_memory_leak
    skip "OMRTODO: Test disabled. This test ends up allocating 1.5gig strings which is generally too large for us to be able to allocate for in a reasonable sized heap (without compaction which we currently do not support)"

    assert_no_memory_leak(["-r-test-/string/string"],
                          <<-PRE, <<-CMD, "rb_str_modify_expand()", limit: 2.5)
      s=Bug::String.new
    PRE
      size = $initial_size
      10.times{s.modify_expand!(size)}
      s.replace("")
    CMD
  end
end
