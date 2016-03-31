/**********************************************************************

  main.c -

  $Author$
  created at: Fri Aug 19 13:19:58 JST 1994

  Copyright (C) 1993-2007 Yukihiro Matsumoto

**********************************************************************/

#undef RUBY_EXPORT
#include "ruby.h"
#include "vm_debug.h"
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef RUBY_DEBUG_ENV
#include <stdlib.h>
#endif

#define RWY_DEBUG 1
#undef RWY_DEBUG

#if defined(RWY_DEBUG)
#include <mcheck.h>
#endif

int
main(int argc, char **argv)
{
  int r = 0;
#if defined(RWY_DEBUG) && 0
  mcheck_pedantic(NULL);
#endif

#ifdef RUBY_DEBUG_ENV
    ruby_set_debug_option(getenv("RUBY_DEBUG"));
#endif
#ifdef HAVE_LOCALE_H
    setlocale(LC_CTYPE, "");
#endif

    ruby_sysinit(&argc, &argv);
    {
	RUBY_INIT_STACK;
	ruby_init();
	r = ruby_run_node(ruby_options(argc, argv));

#if defined(RWY_DEBUG)
  mcheck_check_all();
#endif

  return r;
    }
}
