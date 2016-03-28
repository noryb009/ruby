/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "omrcfg.h"
#include "omr.h"

extern "C" {

#include "omrthread.h"
#include "omrgc.h"
#include "rbgcsupport.h"
#include "thread_api.h"
#include "ruby/ruby.h"
#include "vm_core.h"
#include "vmaccess.h"

} /* extern "C" */

#include "CollectorLanguageInterfaceImpl.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "Heap.hpp"
#include "HeapMapIterator.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"

#include "EnvironmentBase.hpp"
#include "EnvironmentLanguageInterface.hpp"

#define OMR_REALLOC_SHRINK_THRESHOLD 0x16

// rb_omr_markstate is an
struct rb_omr_markstate
{
	void *dummy;
};

rb_omr_markstate_t
rb_omr_get_markstate()
{
	rb_omr_markstate_t ms;
	rb_vm_t * vm = GET_VM();
	ms = (rb_omr_markstate_t) MM_EnvironmentBase::getEnvironment(omr_vmthread_getCurrent(vm->_omrVM));
	return ms;
}

MMINLINE MM_CollectorLanguageInterfaceImpl *
rb_omr_get_cli(MM_GCExtensionsBase * extensions)
{
	return (MM_CollectorLanguageInterfaceImpl *) extensions->collectorLanguageInterface;
}

VALUE
rb_omr_convert_ref_to_id(VALUE object)
{
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(GET_VM()->_omrVM);
	uintptr_t heapBase = (uintptr_t) extensions->heap->getHeapBase();
	uintptr_t offset = ((uintptr_t)object) - heapBase;
	return (VALUE) (offset << 1) & ~0b1111;
}

VALUE
rb_omr_convert_id_to_ref(VALUE id)
{
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(GET_VM()->_omrVM);
	uintptr_t heapBase = (uintptr_t) extensions->heap->getHeapBase();
	uintptr_t offset = (id >> 1) & ~0b111;
	VALUE object = (VALUE) (offset + heapBase);
	return object;
}

int
rb_omr_is_valid_object(VALUE rubyObject)
{
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(GET_VM()->_omrVM);
	MM_CollectorLanguageInterfaceImpl * cli = rb_omr_get_cli(extensions);
	return cli->isValidRubyObject((languageobjectptr_t) rubyObject);
}

int
rb_omr_mark_valid_object(rb_thread_t * rubyThread, VALUE rubyObject)
{
	OMR_VMThread * omrThread = rubyThread->_omrVMThread;
	MM_EnvironmentBase * env = MM_EnvironmentBase::getEnvironment(omrThread);
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(omrThread->_vm);
	MM_CollectorLanguageInterfaceImpl * cli = rb_omr_get_cli(extensions);
	return cli->markValidRubyObject(env, (languageobjectptr_t) rubyObject);
}

static inline omrobjectptr_t
rb_omr_heap_alloc(rb_thread_t *th, long size)
{
	omrobjectptr_t result = NULL;
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(th->_omrVMThread->_vm);
	long allocSize = extensions->objectModel.adjustSizeInBytes(size);

#undef DEBUG_THREAD_CONCURRENCY
#if defined(DEBUG_THREAD_CONCURRENCY)
	/* Bug #1: The executing thread does not match the passed thread */
	MM_EnvironmentBase *actualEnv = MM_EnvironmentBase::getEnvironment(omr_vmthread_getCurrent(GET_VM()->_omrVM));
	MM_EnvironmentBase *passedEnv = MM_EnvironmentBase::getEnvironment(th->_omrVMThread);
	Assert_MM_true(actualEnv == passedEnv); /* May happen if allocating outside GVL without proper checks. */
	/* Bug #2: Someone else has already obtained exclusive, we may be allocating during a GC. */
	if (RUBY_XACCESS_NONE != GET_VM()->exclusiveAccessState) {
		OMR_VMThread * listThread = th->_omrVMThread;
		while ((listThread = listThread->_linkNext) != th->_omrVMThread) {
			rb_thread_t *thr = (rb_thread_t*) listThread->_language_vmthread;
			fprintf(stderr,"omrVMThread [%p] flags [0x%lX]\n",thr->_omrVMThread, thr->publicFlags);
		}
		Assert_MM_true(false); /* Someone else was holding the lock, abort. */
	}
#endif /* defined(DEBUG_THREAD_CONCURRENCY) */

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	uint8_t *heapAlloc = th->omrTlh.heapAlloc;
	uintptr_t afterAlloc = (uintptr_t)heapAlloc + allocSize;

	if (afterAlloc <= (uintptr_t)th->omrTlh.heapTop) {
		th->omrTlh.heapAlloc = (uint8_t *)afterAlloc;
#if defined(OMR_GC_TLH_PREFETCH_FTA)
		th->omrTlh.tlhPrefetchFTA -= allocSize;
#endif  /* OMR_GC_TLH_PREFETCH_FTA */
		result = (omrobjectptr_t) heapAlloc;
#endif  /* OMR_GC_THREAD_LOCAL_HEAP */
	}

	if (NULL == result) {
		/* TODO: respect ready_to_gc in objspace */
		if (ready_to_gc(GET_VM()->objspace)) {
			result = OMR_GC_Allocate(th->_omrVMThread, allocSize, OMR_GC_ALLOCATE_NO_FLAGS);
		} else {
			result = OMR_GC_AllocateNoGC(th->_omrVMThread, allocSize, OMR_GC_ALLOCATE_NO_FLAGS);
		}
		if(NULL == result) {
			fprintf(stderr, "[FATAL] failed to allocate memory\n");
			exit(EXIT_FAILURE);
		}
	}

	return result;
}

 /*
  * Heap iteration code.
  */

VALUE
rb_omr_get_freeobj(rb_thread_t * th, size_t size)
{
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(th->_omrVMThread->_vm);
	size_t sizeWithHeader = size + extensions->objectModel.getSizeInBytesObjectHeader();
	omrobjectptr_t omrObject = rb_omr_heap_alloc(th, sizeWithHeader);
	languageobjectptr_t rubyObject = extensions->objectModel.convertOmrObjectToLanguageObject(omrObject);
	VALUE o = (VALUE) rubyObject;
	return (VALUE) o;
}

void
rb_omr_iterate_all_objects(void *objspace, OMR_VMThread *omrVMThread, void(*func)(VALUE p, void *objspace))
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);

	rb_omr_prepare_heap_for_walk();

	MM_HeapRegionManager *regionManager = extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);

	MM_HeapRegionDescriptorStandard *hrd = NULL;
	while (NULL != (hrd = regionIterator.nextRegion())) {

		MM_HeapMapIterator validObjectIterator(extensions,
			extensions->getObjectMap()->getMarkMap(),
			(uintptr_t*)hrd->getLowAddress(),
			(uintptr_t*)hrd->getHighAddress());

		omrobjectptr_t omrobjptr = NULL;

		while (NULL != (omrobjptr = validObjectIterator.nextObject())) {

			languageobjectptr_t rubyobjptr = extensions->objectModel.convertOmrObjectToLanguageObject(omrobjptr);

			if (T_OMRBUF != BUILTIN_TYPE(rubyobjptr)) {
				func((VALUE)rubyobjptr, objspace);
			}
			validObjectIterator.setHeapMap(extensions->getObjectMap()->getMarkMap());
		}
	}
}

typedef int each_obj_callback(void * pstart, void * pend, size_t size, void * data);

struct each_obj_args {
    each_obj_callback *callback;
    void *data;
};

static omrobjectptr_t lastomrobjptr __attribute__((used));

void
rb_omr_each_objects(VALUE arg)
{
	OMR_VMThread * omrVMThread = GET_THREAD()->_omrVMThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);

	each_obj_args * args = (struct each_obj_args *)arg;

	rb_omr_prepare_heap_for_walk();

	MM_HeapRegionManager *regionManager = extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);

	MM_HeapRegionDescriptorStandard *hrd = NULL;
	while (NULL != (hrd = regionIterator.nextRegion())) {

		MM_HeapMapIterator validObjectIterator(extensions,
			extensions->getObjectMap()->getMarkMap(),
			(uintptr_t*)hrd->getLowAddress(),
			(uintptr_t*)hrd->getHighAddress());

		omrobjectptr_t omrobjptr = NULL;

		while (NULL != (omrobjptr = validObjectIterator.nextObject())) {

			languageobjectptr_t rubyobjptr = extensions->objectModel.convertOmrObjectToLanguageObject(omrobjptr);

			if (T_OMRBUF != BUILTIN_TYPE(rubyobjptr)) {
				uintptr_t size = extensions->objectModel.getSizeInBytesWithoutHeader(omrobjptr);
				if ((*args->callback)(rubyobjptr, (void*)(((uintptr_t)rubyobjptr) + size), size, args->data)) {
					break;
				}
				validObjectIterator.setHeapMap(extensions->getObjectMap()->getMarkMap());
			}
		}
	}
}

void
rb_omr_count_objects(size_t * counts, size_t * freed, size_t * total)
{
	OMR_VMThread * omrVMThread = GET_THREAD()->_omrVMThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);

	rb_omr_prepare_heap_for_walk();

	MM_HeapRegionManager *regionManager = extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIteratorStandard regionIterator(regionManager);

	MM_HeapRegionDescriptorStandard *hrd = NULL;
	while (NULL != (hrd = regionIterator.nextRegion())) {

		MM_HeapMapIterator validObjectIterator(extensions,
			extensions->getObjectMap()->getMarkMap(),
			(uintptr_t*)hrd->getLowAddress(),
			(uintptr_t*)hrd->getHighAddress());

		omrobjectptr_t omrobjptr = NULL;

		while (NULL != (omrobjptr = validObjectIterator.nextObject())) {

			languageobjectptr_t rubyobjptr = extensions->objectModel.convertOmrObjectToLanguageObject(omrobjptr);

			if (T_OMRBUF != BUILTIN_TYPE(rubyobjptr)) {
				counts[ruby_type_to_index(BUILTIN_TYPE(rubyobjptr))] += 1;
				*total += 1;
			}
		}
	}
	*freed = 0;
}

void mark_omrBufStruct(rb_omr_markstate_t ms, void *data)
{
	omr_mark_leaf_object(ms, OMRBUF_HEAP_PTR_FROM_DATA_PTR(data));
}

void
omr_mark_leaf_object(rb_omr_markstate_t ms, languageobjectptr_t object)
{
	assert(ms != NULL);
	MM_EnvironmentBase * env = (MM_EnvironmentBase *)ms;

	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(((MM_EnvironmentBase*)env)->getOmrVM());
	MM_CollectorLanguageInterfaceImpl *cli = (MM_CollectorLanguageInterfaceImpl*)(extensions->collectorLanguageInterface);

	/* mark object as leaf */
	cli->markingScheme_markLeafObject((MM_EnvironmentBase*)env, extensions->objectModel.convertLanguageObjectToOmrObject(object));
}

void
omr_mark_object(rb_omr_markstate_t ms, languageobjectptr_t object)
{
	assert (ms != NULL);
	MM_EnvironmentBase * env = (MM_EnvironmentBase *)ms;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(((MM_EnvironmentBase*)env)->getOmrVM());
	MM_CollectorLanguageInterfaceImpl *cli = (MM_CollectorLanguageInterfaceImpl*)(extensions->collectorLanguageInterface);

	cli->markingScheme_markObject((MM_EnvironmentBase*)env, extensions->objectModel.convertLanguageObjectToOmrObject(object));
}

bool
omr_is_object_marked(languageobjectptr_t object)
{
	MM_EnvironmentBase * env = MM_EnvironmentBase::getEnvironment(GET_THREAD()->_omrVMThread);
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(((MM_EnvironmentBase*)env)->getOmrVM());
	MM_CollectorLanguageInterfaceImpl * cli = rb_omr_get_cli(extensions);
	return cli->markingScheme_isMarked(extensions->objectModel.convertLanguageObjectToOmrObject(object));
}
/*
 * More or less a replacement for malloc(), but using on-heap allocation
 */
 /* Allocate an on-heap omrbuffer. Returns the heap pointer (omrbuffer header address). */
static inline OMRBuffer *
alloc_omr_buffer_object(rb_thread_t *rubyThread, long bufSize)
{
	OMR_VMThread * omrVmThread = rubyThread->_omrVMThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVmThread->_vm);

	/* Ensure the buffer size is aligned before allocation. */
	long size = extensions->objectModel.getAdjustedSizeInBytesForOMRBuf(bufSize);
	omrobjectptr_t omrObjectPtr = rb_omr_heap_alloc(rubyThread, size);
	OMRBuffer *buf = (OMRBuffer *)extensions->objectModel.convertOmrObjectToLanguageObject(omrObjectPtr);

	buf->basic.flags = RUBY_T_OMRBUF;
	buf->size = extensions->objectModel.getAdjustedSizeInBytesWithoutOMRBufHeader(bufSize);
	return buf;
}

static void *
alloc_omr_buffer_internal(rb_thread_t *rubyThread, long bufSize)
{
	OMRBuffer * buf = alloc_omr_buffer_object(rubyThread, bufSize);
	return OMRBUF_DATA_PTR_FROM_HEAP_PTR(buf);
}

static void *
zalloc_omr_buffer_internal(rb_thread_t *rubyThread, long bufSize)
{
	OMRBuffer * buf = alloc_omr_buffer_object(rubyThread, bufSize);
	memset(OMRBUF_DATA_PTR_FROM_HEAP_PTR(buf), 0, bufSize);
	return OMRBUF_DATA_PTR_FROM_HEAP_PTR(buf);
}

static inline void *
shrink_omr_buffer(rb_thread_t *rubyThread, OMRBuffer * oldOMRBuffer, long newDataSize)
{
	assert(oldOMRBuffer->size >= newDataSize);

	/* Adding holes after shrinking OMR buffer ensures heap walk will not fail for finding empty space. */
	MM_GCExtensionsBase * extensions = MM_GCExtensionsBase::getExtensions(rubyThread->_omrVMThread->_vm);
	uintptr_t oldBufferSize = extensions->objectModel.getConsumedSizeInBytesWithHeader(oldOMRBuffer);
	uintptr_t newBufferSize = extensions->objectModel.getAdjustedSizeInBytesForOMRBuf(newDataSize);

	uintptr_t holeSize = oldBufferSize - newBufferSize;

	if (holeSize >= OMR_REALLOC_SHRINK_THRESHOLD) {
		MM_HeapLinkedFreeHeader::fillWithHoles((void *) (((uintptr_t)oldOMRBuffer) + newBufferSize), holeSize);
		oldOMRBuffer->size = extensions->objectModel.getAdjustedSizeInBytesWithoutOMRBufHeader(newDataSize);
	}

	/* Just give the old buffer back. */
	return OMRBUF_DATA_PTR_FROM_HEAP_PTR(oldOMRBuffer);
}

static inline void *
grow_omr_buffer(rb_thread_t *rubyThread, OMRBuffer * oldOMRBuffer, long newDataSize)
{
	assert(oldOMRBuffer->size < newDataSize);

	char *newBuffer = (char *)alloc_omr_buffer_internal(rubyThread, newDataSize);
	memcpy(newBuffer, OMRBUF_DATA_PTR_FROM_HEAP_PTR(oldOMRBuffer),
	       oldOMRBuffer->size);
	return newBuffer;
}

static void *
realloc_omr_buffer_internal(rb_thread_t *rubyThread, void * oldBuffer, long newDataSize)
{
	if (UNLIKELY(NULL == oldBuffer)) {
		return alloc_omr_buffer_internal(rubyThread, newDataSize);
	}
	else {
		OMRBuffer *oldOMRBuffer = OMRBUF_HEAP_PTR_FROM_DATA_PTR(oldBuffer);
		if (newDataSize > oldOMRBuffer->size) {
			return grow_omr_buffer(rubyThread, oldOMRBuffer, newDataSize);
		}
		else {
			if (newDataSize != oldOMRBuffer->size) {
				return shrink_omr_buffer(rubyThread, oldOMRBuffer, newDataSize);
			} else {
				return oldBuffer;
			}

		}
	}
}

static void *
calloc_omr_buffer_internal(rb_thread_t *rubyThread, long numElem, long elemSize)
{
	return zalloc_omr_buffer_internal(rubyThread, numElem*elemSize);
}

void *
alloc_omr_buffer(long bufSize)
{
	return alloc_omr_buffer_internal(GET_THREAD(), bufSize);
}

void *
calloc_omr_buffer(long numElem, long elemSize)
{
	return calloc_omr_buffer_internal(GET_THREAD(), numElem, elemSize);
}

void *
realloc_omr_buffer(void *oldBuf, long newBufSize)
{
	return realloc_omr_buffer_internal(GET_THREAD(), oldBuf, newBufSize);
}

void *
zalloc_omr_buffer(long bufSize)
{
	return zalloc_omr_buffer_internal(GET_THREAD(), bufSize);
}

void *
realloc_omr_buffer_without_gvl(void *oldBuf, long newBufSize)
{
	/* GET_THREAD() will return a stale pointer (the last thread that held the GVL), we want the real thread. */
	rb_thread_t *rubyThread = (rb_thread_t *) omr_vmthread_getCurrent(GET_VM()->_omrVM)->_language_vmthread;
	void *newBuf = NULL;
	acquire_vm_access(rubyThread);
	newBuf = realloc_omr_buffer_internal(rubyThread, oldBuf, newBufSize);
	release_vm_access(rubyThread);
	return newBuf;
}

void
omr_onig_region_mark(rb_omr_markstate_t ms, struct re_registers *regs)
{
	if (regs->allocated > 0) {
		RUBY_MARK_OMRBUF_UNLESS_NULL(ms, regs->beg);
		RUBY_MARK_OMRBUF_UNLESS_NULL(ms, regs->end);
	}
}

int
rb_omr_prepare_heap_for_walk(void)
{
	// cli->flushCachesForGC(environment);
	return (int)OMR_GC_SystemCollect(GET_THREAD()->_omrVMThread,
	                                 // J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT);
	                                 J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC);
}
