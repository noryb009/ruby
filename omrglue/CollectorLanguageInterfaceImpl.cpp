/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

extern "C" {

#include "ruby.h"
#include "ruby/defines.h"

#include "omrcfg.h"
#include "omrmodroncore.h"
#include "mminitcore.h"
#include "omr.h"

#include "constant.h"
#include "internal.h"
#include "ruby/io.h"
#include "ruby_atomic.h"
#include "rbgcsupport.h"
#include "rbomrinit.h"
#include "ruby/st.h"
#include "string.h"
#include "vm_core.h"

} /* extern "C" { */

#include "CollectorLanguageInterfaceImpl.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "HashTableIterator.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "MarkingScheme.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectAllocationInterface.hpp"
#include "OMRVMInterface.hpp"
#include "PoolIterator.hpp"
#include "SublistFragment.hpp"
#include "SublistIterator.hpp"
#include "SublistPool.hpp"
#include "SublistSlotIterator.hpp"
#include "VerboseManagerImpl.hpp"
#include "WorkPacketsStandard.hpp"

#undef OMR_POST_MARK_DEBUG

/**
 * Initialization
 */
MM_CollectorLanguageInterfaceImpl *
MM_CollectorLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_CollectorLanguageInterfaceImpl *cli = NULL;
	OMR_VM *omrVM = env->getOmrVM();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);

	cli = (MM_CollectorLanguageInterfaceImpl *)extensions->getForge()->allocate(sizeof(MM_CollectorLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != cli) {
		new (cli) MM_CollectorLanguageInterfaceImpl(omrVM);
		if (!cli->initialize(omrVM)) {
			cli->kill(env);
			cli = NULL;
		}
	}

	return cli;
}

void
MM_CollectorLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	OMR_VM *omrVM = env->getOmrVM();
	tearDown(omrVM);
	MM_GCExtensionsBase::getExtensions(omrVM)->getForge()->free(this);
}

void
MM_CollectorLanguageInterfaceImpl::tearDown(OMR_VM *omrVM)
{
}

bool
MM_CollectorLanguageInterfaceImpl::initialize(OMR_VM *omrVM)
{
	return true;
}

void
MM_CollectorLanguageInterfaceImpl::flushCachesForGC(MM_EnvironmentBase *env)
{
        GC_OMRVMInterface::flushCachesForGC(env);
}

void
MM_CollectorLanguageInterfaceImpl::flushNonAllocationCaches(MM_EnvironmentBase *env)
{
	MM_SublistFragment::flush((J9VMGC_SublistFragment*) GET_VM()->rDataParallelFinalizeFragment);
}

OMR_VMThread *
MM_CollectorLanguageInterfaceImpl::attachVMThread(OMR_VM *omrVm, const char *threadName, uintptr_t reason)
{
	OMR_VMThread *omrVmThread = NULL;
	omr_error_t rc = OMR_Glue_BindCurrentThread(omrVm, threadName, &omrVmThread);
	return OMR_ERROR_NONE == rc ? omrVmThread : NULL;
}

void
MM_CollectorLanguageInterfaceImpl::detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrVMThread, uintptr_t reason)
{
	OMR_Glue_UnbindCurrentThread(omrVMThread);
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_masterSetupForGC(MM_EnvironmentBase *env)
{
	rb_vm_t *vm = GET_VM();

	/* Have to adjust the limits before decrementing the counters during omr_obj_free */
	if (0 != vm->enableMallocGC) {
		adjustMallocLimits();
	}

	/**
	 * Increment the GC count.
	 */
	omr_increment_objspace_profile_count();
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_scanRoots(MM_EnvironmentBase *env)
{
	rb_omr_markstate_t ms = (rb_omr_markstate_t)env;

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		omr_mark_roots_vm(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		omr_mark_roots_tbl(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		rb_gc_mark_encodings(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		omr_mark_roots_global_var(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		rb_mark_end_proc(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		rb_gc_mark_global_tbl(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* Mark the contents of the generic ivar tbl. These are on-heap
		 * per-instance variable tables. */
		rb_mark_generic_ivar_tbl(ms);
	}


	for(int i = 0; i < IVAR_TBL_COUNT; i+= 1) {
		/* Mark the internal buffers of the generic ivar tbl. */
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env) && generic_iv_tbls[i]->heap_allocated) {
			rb_omr_mark_st_table(ms, generic_iv_tbls[i]);
		}
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		omr_mark_roots_unlinked_live_method_entries(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* Ensure we keep objects on the deferred list alive */
		omr_mark_deferred_list(ms);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* Ensure we keep objects on the deferred list alive */
		omr_mark_deferred_list_running(ms);
	}

	/* The current thread stack corresponds to the master thread,
	 * not the slaves. This call should always happen last. */
	if (env->isMasterThread()) {
		omr_mark_roots_current_machine_context(ms);
	}
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_completeMarking(MM_EnvironmentBase *env)
{
}


uintptr_t
MM_CollectorLanguageInterfaceImpl::markingScheme_scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason)
{
	if (SCAN_REASON_PACKET == reason) {
		MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
		omr_mark_children((rb_omr_markstate_t)env, (VALUE)extensions->objectModel.convertOmrObjectToLanguageObject((omrobjectptr_t)objectPtr));
	}
	return env->getExtensions()->objectModel.getSizeInBytesWithHeader(objectPtr);
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
uintptr_t
MM_CollectorLanguageInterfaceImpl::markingScheme_scanObjectWithSize(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MarkingSchemeScanReason reason, uintptr_t sizeToDo)
{
	/* TODO: How does sizeToDo translate into Ruby object slots? For now assume SCAN_MAX. */
	return markingScheme_scanObject(env, objectPtr, MM_CollectorLanguageInterface::SCAN_REASON_PACKET);
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

void
MM_CollectorLanguageInterfaceImpl::parallelDispatcher_handleMasterThread(OMR_VMThread *omrVMThread)
{
	/* Do nothing for now.  only required for SRT */
}

uintptr_t
MM_CollectorLanguageInterfaceImpl::postMarkRegion(GC_ObjectHeapIterator &objectIterator)
{
	uintptr_t count = 0;
	omrobjectptr_t omrobjptr = NULL;
	while (NULL != (omrobjptr = objectIterator.nextObject())) {
		assert(_extensions->getObjectMap()->isValidObject(omrobjptr));
		if (!_markingScheme->isMarked(omrobjptr)) {

			/* object will be collected. We write the full contents of the object with a known value. */
			uintptr_t objsize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(omrobjptr);
			memset(omrobjptr,0x5E,(size_t) objsize);
			MM_HeapLinkedFreeHeader::fillWithHoles(omrobjptr, objsize);
		}
	}
	return count;
}

void
MM_CollectorLanguageInterfaceImpl::postMarkCheck(MM_EnvironmentBase *env)
{
	/* this puts the heap into the state required to walk it */
	flushCachesForGC(env);

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);

	/* walk the heap, for objects that are not marked we corrupt them to maximize the chance we will crash immediately
	   if they are used.  For live objects validate that they have the expected eyecatcher */
	int count = 0;
	MM_HeapRegionDescriptor *hrd = NULL;
	while (NULL != (hrd = regionIterator.nextRegion())) {
		/* walk all of the object, make sure that those that were not marked are no longer
		   usable such that if they are later used we will know this and optimally crash */
		GC_ObjectHeapIteratorAddressOrderedList objectIterator(_extensions, hrd, false);
		count += postMarkRegion(objectIterator);
	}
}

void
MM_CollectorLanguageInterfaceImpl::parallelGlobalGC_postMarkProcessing(MM_EnvironmentBase *env)
{
	rb_vm_t *vm = GET_VM();

#if defined(OMR_POST_MARK_DEBUG)
	postMarkCheck(env);
#endif

	/* TODO: investigate whether this is expensive or not.  If it is we should run it in parallel somewhere else */
	if (NULL != vm->postponed_job_buffer) {
		gc_finalize_deferred_register();
	}

	/* finalizer cleanup stuff */
	((MM_SublistPool *)vm->rDataParallelFinalizeList)->compact(env);
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
MM_ConcurrentSafepointCallback*
MM_CollectorLanguageInterfaceImpl::concurrentGC_createSafepointCallback(MM_EnvironmentBase *env)
{
	/* Placeholder to satisfy compile-time requirements */
	Assert_MM_unreachable();
	return NULL;
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

void
MM_CollectorLanguageInterfaceImpl::parallelGlobalGC_masterThreadGarbageCollect_gcComplete(MM_EnvironmentBase *env, bool didCompact)
{
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::makeZombie(MM_EnvironmentBase * env, VALUE handle, void (*dfree)(void *), void *data)
{
	omrobjectptr_t omrObject = _extensions->objectModel.convertLanguageObjectToOmrObject(RANY(handle));
	_markingScheme->inlineMarkObjectNoCheck(env, omrObject, true);
	make_zombie(GET_VM()->objspace, (VALUE)handle, dfree, data);
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::makeIoZombie(MM_EnvironmentBase * env, VALUE handle)
{
	rb_io_t *fptr = RANY(handle)->as.file.fptr;
	makeZombie(env, handle, (void (*)(void*))rb_io_fptr_finalize, fptr);
}


MMINLINE void
MM_CollectorLanguageInterfaceImpl::freeClassOrModule(MM_EnvironmentBase * env, VALUE obj)
{
	rb_omr_free_class_or_module(obj);
	doDefinedFinalization(env, obj);
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::freeDynamicSymbol(MM_EnvironmentBase * env, VALUE obj)
{
	rb_gc_free_dsymbol(obj);
}

/* utility structure for iterating ruby hash tables. */
struct st_foreach_clir_data
{
	MM_MarkingScheme * _markingScheme;
	GC_ObjectModel * _objectModel;
};

MMINLINE void
MM_CollectorLanguageInterfaceImpl::freeData(MM_EnvironmentBase * env, VALUE obj)
{
	int doFinalization = 1;
	if (DATA_PTR(obj)) {
		doFinalization = rb_omr_free_data(obj);
	}
	if (doFinalization) {
		doDefinedFinalization(env, obj);
	} else {
		makeZombie(env, obj, RDATA(obj)->dfree, DATA_PTR(obj));
	}
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::freeFile(MM_EnvironmentBase * env, VALUE obj)
{
	if (RANY(obj)->as.file.fptr) {
		makeIoZombie(env, obj);
	}
	else {
		doDefinedFinalization(env, obj);
	}
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::freeIClass(MM_EnvironmentBase * env, VALUE obj)
{
	rb_omr_free_iclass(obj);
	doDefinedFinalization(env, obj);
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::doDefinedFinalization(MM_EnvironmentBase * env, VALUE obj)
{
	if (J9_UNEXPECTED(FL_TEST(obj, FL_FINALIZE))) {
		RDATA(obj)->dfree = 0;
		makeZombie(env, obj, 0, DATA_PTR(obj));
	}
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::doDefinedFinalizationWrapper(MM_EnvironmentBase * env, VALUE obj)
{
	doDefinedFinalization(env, obj);
}

void
MM_CollectorLanguageInterfaceImpl::callDefinedFinalizersForType(MM_EnvironmentBase * env, enum ruby_value_type type)
{
	callFinalizersForType(env, type,  &MM_CollectorLanguageInterfaceImpl::doDefinedFinalizationWrapper);
}

extern "C" {
static int
freeInstanceVariableTableIfUnmarked(st_data_t key, st_data_t val, st_data_t arg)
{
	st_foreach_clir_data * data = (st_foreach_clir_data *) arg;

	if (SPECIAL_CONST_P(key)) {
		return ST_CONTINUE;
	}

	languageobjectptr_t rubyObject = (languageobjectptr_t) key;
	omrobjectptr_t omrObject = data->_objectModel->convertLanguageObjectToOmrObject(rubyObject);

	if (!data->_markingScheme->isHeapObject(omrObject)) {
		return ST_CONTINUE;
	}

	if (data->_markingScheme->isMarked(omrObject)) {
		return ST_CONTINUE;
	} else {
#if 0
		/* Do we need to do this?  Seems like unneeded writing to the heap */
		FL_UNSET(key, FL_EXIVAR); /* This will corrupt the object table if the handle is freed. */
#endif
		return ST_DELETE;
	}
}
} /* extern "C" */

MMINLINE void
MM_CollectorLanguageInterfaceImpl::freeInstanceVariableTables(MM_EnvironmentBase *env)
{
	st_foreach_clir_data arg;
	arg._markingScheme = _markingScheme;
	arg._objectModel = &_extensions->objectModel;

#if defined(SPLIT_IVAR_TBL)
	for (int i = 0; i < IVAR_TBL_COUNT; i++) {
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			st_table *generic_iv_tbl = generic_iv_tbls[i];
			if (NULL != generic_iv_tbl) {
				st_foreach_inline(generic_iv_tbl, freeInstanceVariableTableIfUnmarked, (st_data_t) &arg);
			}
		}
	}
#else
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		st_foreach_inline(generic_iv_tbl, freeInstanceVariableTableIfUnmarked, (st_data_t) &arg);
	}
#endif


}

void
MM_CollectorLanguageInterfaceImpl::callFinalizersForType(MM_EnvironmentBase * env, enum ruby_value_type type, freeObjFunction freeObjFunction)
{
	/* Wrapping this, really want to have freeObjFunction inlined and the only way to have the compiler do that is to have
	 * inlineCallFinalizersForType be inline.  If we call inlineCallFinalizersForType() directly though compilers will
	 * complain that functions (markingScheme_markLiveObjectsComplete()) get too large.  So wrap inlineCallFinalizersForType()
	 * inside a non-inline function.
	 */
	inlineCallFinalizersForType(env, type, freeObjFunction);
}

MMINLINE void
MM_CollectorLanguageInterfaceImpl::inlineCallFinalizersForType(MM_EnvironmentBase * env, enum ruby_value_type type, freeObjFunction freeObjFunction)
{
	rb_vm_t * vm = GET_VM();
	/* Using markmap directly saves us a heap check and an extra indirect off markingscheme. */
	MM_MarkMap * markMap = _markingScheme->getMarkMap();
	languageobjectptr_t * slot = NULL;
	J9Pool *finalizeList = vm->finalizeList[ruby_type_to_index(type)];
	GC_PoolIterator finalizeIterator(finalizeList);

	while(NULL != (slot = (languageobjectptr_t *)finalizeIterator.nextSlot())) {
		if (!markMap->isBitSet(_extensions->objectModel.convertLanguageObjectToOmrObject((languageobjectptr_t)RANY(*slot)))) {
			/* TODO: Remove an object from the finalize list when it is recycled. ~RY */
			if (BUILTIN_TYPE(*slot) != T_NONE) {
				(this->*freeObjFunction)(env, (VALUE)*slot);
			}
			pool_removeElement(finalizeList, (void*)slot);
		}
	}
}

void
MM_CollectorLanguageInterfaceImpl::callFinalizersInParallelForData(MM_EnvironmentBase * env)
{
	MM_SublistPuddle *puddle;
	rb_vm_t * vm = GET_VM();
	GC_SublistIterator finalizeListPuddleIterator((MM_SublistPool *)vm->rDataParallelFinalizeList);
	MM_MarkMap * markMap = _markingScheme->getMarkMap();

	while((puddle = finalizeListPuddleIterator.nextList()) != NULL) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			GC_SublistSlotIterator slotIterator(puddle);
			VALUE *handlePtr;
			while((handlePtr = (VALUE *)slotIterator.nextSlot()) != NULL) {
				if (0 != *handlePtr) {
					if (!markMap->isBitSet(_extensions->objectModel.convertLanguageObjectToOmrObject((languageobjectptr_t)RANY(*handlePtr)))) {
						freeData(env, (VALUE)*handlePtr);
						slotIterator.removeSlot();
					}
				} else {
					slotIterator.removeSlot();
				}
			}
		}
	}
}

#if defined(OMR_DEBUG)
static bool
typeHasFinalizers(enum ruby_value_type type)
{
	J9Pool * finalizeList = GET_VM()->finalizeList[ruby_type_to_index(type)];
	return (0 < pool_numElements(finalizeList));
}
#endif /* OMR_DEBUG */

void
MM_CollectorLanguageInterfaceImpl::callFinalizers(MM_EnvironmentBase * env)
{
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_OBJECT);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		/* TODO: find out why are T_SYMBOLS and T_STRINGS done syncronously with eachother ? */
		callFinalizersForType(env, T_SYMBOL, &MM_CollectorLanguageInterfaceImpl::freeDynamicSymbol);
		callDefinedFinalizersForType(env, T_STRING);
	}

	/*TODO: using per-object finalizers is pointless now.. remove them */
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_REGEXP);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_ARRAY);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_HASH);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_STRUCT);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_BIGNUM);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callFinalizersForType(env, T_FILE, &MM_CollectorLanguageInterfaceImpl::freeFile);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callFinalizersForType(env, T_DATA, &MM_CollectorLanguageInterfaceImpl::freeData);
	}

	callFinalizersInParallelForData(env);

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_MATCH);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_RATIONAL);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_COMPLEX);
	}

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callDefinedFinalizersForType(env, T_NODE);
	}

	/* These types can't be run in parallel with each other for now since they do stuff like
	 * rb_class_remove_from_module_subclasses which modifies shared class hierarchies.
	 */
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		callFinalizersForType(env, T_MODULE, &MM_CollectorLanguageInterfaceImpl::freeClassOrModule);
		callFinalizersForType(env, T_CLASS, &MM_CollectorLanguageInterfaceImpl::freeClassOrModule);
		callFinalizersForType(env, T_ICLASS, &MM_CollectorLanguageInterfaceImpl::freeIClass);
	}

#if defined(OMR_DEBUG)
	/* Assert all types which shouldn't have finalizers _really_
	 * don't have any.  */
	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		assert(!typeHasFinalizers(T_NONE));
		assert(!typeHasFinalizers(T_NIL));
		assert(!typeHasFinalizers(T_FLOAT));
		assert(!typeHasFinalizers(T_FIXNUM));
		assert(!typeHasFinalizers(T_TRUE));
		assert(!typeHasFinalizers(T_FALSE));
		assert(!typeHasFinalizers(T_TRUE));
		assert(!typeHasFinalizers(T_SYMBOL));
		assert(!typeHasFinalizers(T_UNDEF));
		assert(!typeHasFinalizers(T_ZOMBIE));
		assert(!typeHasFinalizers(T_OMRBUF));
	}
#endif /* defined(OMR_DEBUG) */
}

static MMINLINE int
scanFrozenString(st_data_t key, st_data_t val, st_data_t arg)
{
	st_foreach_clir_data * data = (st_foreach_clir_data *) arg;
	VALUE str = (VALUE)key;

	omrobjectptr_t objectPtr = data->_objectModel->convertLanguageObjectToOmrObject(RANY(str));

	assert(FL_TEST(str, RSTRING_FSTR));

	if (!data->_markingScheme->isMarked(objectPtr)) {
		return ST_DELETE;
	} else {
		return ST_CONTINUE;
	}
}

/* TODO: look into if this is better/faster or if adding frozen strings to the finalize list is.
 * Same thing with iVars.  Is walking the finalize list cheaper than walking the tables?
 * */

void
MM_CollectorLanguageInterfaceImpl::freeFrozenStrings(MM_EnvironmentBase *env)
{
	st_foreach_clir_data arg;
	arg._markingScheme = _markingScheme;
	arg._objectModel = &_extensions->objectModel;

	st_foreach_inline(rb_vm_fstring_table(), scanFrozenString, (st_data_t) &arg);
}

void
MM_CollectorLanguageInterfaceImpl::markingScheme_markLiveObjectsComplete(MM_EnvironmentBase *env)
{
	callFinalizers(env);

	if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
		freeFrozenStrings(env);
	}

	freeInstanceVariableTables(env);
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_CollectorLanguageInterfaceImpl::scavenger_reportObjectEvents(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterSetupForGC(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_workerSetupForGC_clearEnvironmentLangStats(MM_EnvironmentBase *env)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_reportScavengeEnd(MM_EnvironmentBase * envBase, bool scavengeSuccessful)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_mergeGCStats_mergeLangStats(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeComplete(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

void
MM_CollectorLanguageInterfaceImpl::scavenger_masterThreadGarbageCollect_scavengeSuccess(MM_EnvironmentBase *envBase)
{
	/* Do nothing for now */
}

bool
MM_CollectorLanguageInterfaceImpl::scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(MM_EnvironmentBase *envBase, PercolateReason *reason, uint32_t *gcCode)
{
	/* Do nothing for now */
	return false;
}

GC_ObjectScanner *
MM_CollectorLanguageInterfaceImpl::scavenger_getObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags)
{
	return NULL;
}

void 
MM_CollectorLanguageInterfaceImpl::scavenger_flushReferenceObjects(MM_EnvironmentStandard *env)
{
	/* Do nothing for now */
}

bool 
MM_CollectorLanguageInterfaceImpl::scavenger_hasIndirectReferentsInNewSpace(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	return false;
}

bool 
MM_CollectorLanguageInterfaceImpl::scavenger_scavengeIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	return false;
}

void 
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjectSlots(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
	/* Do nothing for now */
}

void 
MM_CollectorLanguageInterfaceImpl::scavenger_backOutIndirectObjects(MM_EnvironmentStandard *env)
{
	/* Do nothing for now */
}

void 
MM_CollectorLanguageInterfaceImpl::scavenger_reverseForwardedObject(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject)
{
	/* Do nothing for now */
}

#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
void 
MM_CollectorLanguageInterfaceImpl::scavenger_fixupDestroyedSlot(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedObject, MM_MemorySubSpaceSemiSpace *subSpaceNew)
{
	/* Do nothing for now */
}
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
#endif /* OMR_GC_MODRON_SCAVENGER */

#if defined(OMR_POST_MARK_DEBUG)
void
MM_CollectorLanguageInterfaceImpl::verifyHeapRegion(GC_ObjectHeapIterator &objectIterator)
{
	omrobjectptr_t omrobjptr = NULL;
	while (NULL != (omrobjptr = objectIterator.nextObject())) {
		if (!_markingScheme->isMarked(omrobjptr)) {
			/* object will be collected. We write the full contents of the object with a known value  */
			uintptr_t objsize = _extensions->objectModel.getSizeInBytesWithHeader(omrobjptr);
			memset(omrobjptr,0x5E,(size_t) objsize);
			MM_HeapLinkedFreeHeader::fillWithHoles(omrobjptr, objsize);
		}
	}
}
#endif

#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_CollectorLanguageInterfaceImpl::compactScheme_verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap)
{
#error implement heap verification for compact
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_fixupRoots(MM_EnvironmentBase *env)
{
	assert(0);
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_workerCleanupAfterGC(MM_EnvironmentBase *env)
{
}

void
MM_CollectorLanguageInterfaceImpl::compactScheme_languageMasterSetupForGC(MM_EnvironmentBase *env)
{
}
#endif /* OMR_GC_MODRON_COMPACTION */

omrobjectptr_t
MM_CollectorLanguageInterfaceImpl::heapWalker_heapWalkerObjectSlotDo(omrobjectptr_t object)
{
	return NULL;
}

bool
MM_CollectorLanguageInterfaceImpl::markingScheme_markObject(MM_EnvironmentBase* env, omrobjectptr_t object)
{
	return _markingScheme->markObject(env, object, false);
}

bool
MM_CollectorLanguageInterfaceImpl::markingScheme_markLeafObject(MM_EnvironmentBase * env, omrobjectptr_t object)
{
	return _markingScheme->markObject(env, object, true);
}

bool
MM_CollectorLanguageInterfaceImpl::markingScheme_isMarked(omrobjectptr_t object)
{
	return _markingScheme->isMarked(object);
}
