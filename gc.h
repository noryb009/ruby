
#ifndef RUBY_GC_H
#define RUBY_GC_H 1

#if defined(__cplusplus)
extern "C" {
#endif

/* OMR: Included because of RVALUE move into .h file */
#include "ruby/ruby.h"
#include "ruby/re.h"
#include "node.h"
#include "objectdescription.h"
#include "vm_core.h"

#if defined(__x86_64__) && !defined(_ILP32) && defined(__GNUC__) && !defined(__native_client__)
#define SET_MACHINE_STACK_END(p) __asm__ __volatile__ ("movq\t%%rsp, %0" : "=r" (*(p)))
#elif defined(__i386) && defined(__GNUC__) && !defined(__native_client__)
#define SET_MACHINE_STACK_END(p) __asm__ __volatile__ ("movl\t%%esp, %0" : "=r" (*(p)))
#else
NOINLINE(void rb_gc_set_stack_end(VALUE **stack_end_p));
#define SET_MACHINE_STACK_END(p) rb_gc_set_stack_end(p)
#define USE_CONSERVATIVE_STACK_END
#endif

/* for GC debug */

#ifndef RUBY_MARK_FREE_DEBUG
#define RUBY_MARK_FREE_DEBUG 0
#endif

#if RUBY_MARK_FREE_DEBUG
extern int ruby_gc_debug_indent;

static inline void
rb_gc_debug_indent(void)
{
    printf("%*s", ruby_gc_debug_indent, "");
}

static inline void
rb_gc_debug_body(const char *mode, const char *msg, int st, void *ptr)
{
    if (st == 0) {
	ruby_gc_debug_indent--;
    }
    rb_gc_debug_indent();
    printf("%s: %s %s (%p)\n", mode, st ? "->" : "<-", msg, ptr);

    if (st) {
	ruby_gc_debug_indent++;
    }

    fflush(stdout);
}

#define RUBY_MARK_ENTER(msg) rb_gc_debug_body("mark", (msg), 1, ptr)
#define RUBY_MARK_LEAVE(msg) rb_gc_debug_body("mark", (msg), 0, ptr)
#define RUBY_FREE_ENTER(msg) rb_gc_debug_body("free", (msg), 1, ptr)
#define RUBY_FREE_LEAVE(msg) rb_gc_debug_body("free", (msg), 0, ptr)
#define RUBY_GC_INFO         rb_gc_debug_indent(); printf

#else
#define RUBY_MARK_ENTER(msg)
#define RUBY_MARK_LEAVE(msg)
#define RUBY_FREE_ENTER(msg)
#define RUBY_FREE_LEAVE(msg)
#define RUBY_GC_INFO if(0)printf
#endif

#define RUBY_MARK_UNLESS_NULL(ptr) if(RTEST(ptr)){rb_gc_mark(ptr);}
#define RUBY_OMR_MARK_UNLESS_NULL(ms, ptr) if(RTEST(ptr)){rb_omr_mark(ms, ptr);}
#define RUBY_MARK_OMRBUF(ms, ptr) omr_mark_leaf_object(ms, (languageobjectptr_t)OMRBUF_HEAP_PTR_FROM_DATA_PTR(ptr))
#define RUBY_MARK_OMRBUF_UNLESS_NULL(ms, ptr) if (NULL != ptr) {RUBY_MARK_OMRBUF(ms, ptr);}

#define RUBY_FREE_UNLESS_NULL(ptr) if(ptr){ruby_xfree(ptr);(ptr)=NULL;}

#if STACK_GROW_DIRECTION > 0
# define STACK_UPPER(x, a, b) (a)
#elif STACK_GROW_DIRECTION < 0
# define STACK_UPPER(x, a, b) (b)
#else
RUBY_EXTERN int ruby_stack_grow_direction;
int ruby_get_stack_grow_direction(volatile VALUE *addr);
# define stack_growup_p(x) (			\
	(ruby_stack_grow_direction ?		\
	 ruby_stack_grow_direction :		\
	 ruby_get_stack_grow_direction(x)) > 0)
# define STACK_UPPER(x, a, b) (stack_growup_p(x) ? (a) : (b))
#endif

#if STACK_GROW_DIRECTION
#define STACK_GROW_DIR_DETECTION
#define STACK_DIR_UPPER(a,b) STACK_UPPER(0, (a), (b))
#else
#define STACK_GROW_DIR_DETECTION VALUE stack_grow_dir_detection
#define STACK_DIR_UPPER(a,b) STACK_UPPER(&stack_grow_dir_detection, (a), (b))
#endif
#define IS_STACK_DIR_UPPER() STACK_DIR_UPPER(1,0)

RUBY_SYMBOL_EXPORT_BEGIN

/* exports for objspace module */
size_t rb_objspace_data_type_memsize(VALUE obj);
void rb_objspace_reachable_objects_from(VALUE obj, void (func)(VALUE, void *), void *data);
void rb_objspace_reachable_objects_from_root(void (func)(const char *category, VALUE, void *), void *data);
int rb_objspace_markable_object_p(VALUE obj);
int rb_objspace_internal_object_p(VALUE obj);
int rb_objspace_marked_object_p(VALUE obj);
int rb_objspace_garbage_object_p(VALUE obj);

void rb_objspace_each_objects(
    int (*callback)(void *start, void *end, size_t stride, void *data),
    void *data);

void rb_objspace_each_objects_without_setup(
    int (*callback)(void *, void *, size_t, void *),
    void *data);

RUBY_SYMBOL_EXPORT_END

struct rb_objspace;
typedef struct rb_objspace rb_objspace_t;

#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__CYGWIN__)
#pragma pack(push, 1) /* magic for reducing sizeof(RVALUE): 24 -> 20 */
#endif

typedef struct RVALUE {
    union {
	struct {
	    VALUE flags;		/* always 0 for freed obj */
	    struct RVALUE *next;
	} free;
	struct RBasic  basic;
	struct RObject object;
	struct RClass  klass;
	struct RFloat  flonum;
	struct RString string;
	struct RArray  array;
	struct RRegexp regexp;
	struct RHash   hash;
	struct RData   data;
	struct RTypedData   typeddata;
	struct RStruct rstruct;
	struct RBignum bignum;
	struct RFile   file;
	struct RNode   node;
	struct RMatch  match;
	struct RRational rational;
	struct RComplex complex;
	struct {
	    struct RBasic basic;
	    VALUE v1;
	    VALUE v2;
	    VALUE v3;
	} values;
    } as;
#if GC_DEBUG
    const char *file;
    VALUE line;
#endif
} RVALUE;

#define RANY(o) ((RVALUE*)(o))

#if defined(_MSC_VER) || defined(__BORLANDC__) || defined(__CYGWIN__)
#pragma pack(pop)
#endif

#if defined(OMR)

struct RZombie {
    struct RBasic basic;
    VALUE next;
    void (*dfree)(void *);
    void *data;
};

#define RZOMBIE(o) (R_CAST(RZombie)o)

struct rb_objspace;

void make_zombie(rb_objspace_t *objspace, VALUE obj, void (*dfree)(void *), void *data);
void make_io_zombie(rb_objspace_t *objspace, VALUE obj);
void gc_finalize_deferred_register(void);

int ready_to_gc(struct rb_objspace * objspace);

void omr_mark_roots_vm(rb_omr_markstate_t ms);
void omr_mark_roots_tbl(rb_omr_markstate_t ms);
void omr_mark_roots_global_var(rb_omr_markstate_t ms);
void omr_mark_roots_current_machine_context(rb_omr_markstate_t ms);
void omr_mark_roots_unlinked_live_method_entries(rb_omr_markstate_t ms);

void omr_mark_deferred_list(rb_omr_markstate_t ms);
void omr_mark_deferred_list_running(rb_omr_markstate_t ms);
void omr_increment_objspace_profile_count(void);
void omr_mark_roots(void);
void omr_mark_children( rb_omr_markstate_t ms, VALUE object);

int omr_obj_free(void * obj);
void adjustMallocLimits(void);
int rb_omr_obj_free(void * obj);
void rb_omr_adjust_malloc_limits(void);

void rb_omr_free_class_or_module(VALUE obj);
void rb_omr_free_iclass(VALUE obj);
int rb_omr_free_data(VALUE obj);

#endif /* defined(OMR) */

typedef enum {
    GPR_FLAG_NONE               = 0x000,
    /* major reason */
    GPR_FLAG_MAJOR_BY_NOFREE    = 0x001,
    GPR_FLAG_MAJOR_BY_OLDGEN    = 0x002,
    GPR_FLAG_MAJOR_BY_SHADY     = 0x004,
    GPR_FLAG_MAJOR_BY_FORCE     = 0x008,
#if RGENGC_ESTIMATE_OLDMALLOC
    GPR_FLAG_MAJOR_BY_OLDMALLOC = 0x020,
#endif
    GPR_FLAG_MAJOR_MASK         = 0x0ff,

    /* gc reason */
    GPR_FLAG_NEWOBJ             = 0x100,
    GPR_FLAG_MALLOC             = 0x200,
    GPR_FLAG_METHOD             = 0x400,
    GPR_FLAG_CAPI               = 0x800,
    GPR_FLAG_STRESS            = 0x1000,

    /* others */
    GPR_FLAG_IMMEDIATE_SWEEP   = 0x2000,
    GPR_FLAG_HAVE_FINALIZE     = 0x4000
} gc_profile_record_flag;

#if defined(__cplusplus)
}
#endif

#endif /* RUBY_GC_H */
