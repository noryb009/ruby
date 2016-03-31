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

extern "C" {
#include "ruby.h"
#include "internal.h"
}

#include "omr.h"
#include "omrvm.h"
#include "rbgcsupport.h"
#include "rbomrinit.h"

#define linkNext _linkNext
#define linkPrevious _linkPrevious
#include "omrlinkedlist.h"
#include "omrport.h"
#include "mminitcore.h"
#include "omrrasinit.h"
#include "omrprofiler.h"
#include "pool_api.h"
#include "rbomrddr.h"
#include "rbomrprofiler.h"
#include "thread_api.h"

#include "AtomicSupport.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectModel.hpp"
#include "omrgcstartup.hpp"
#include "StartupManagerImpl.hpp"
#include "GCExtensionsBase.hpp"
#include "VerboseManagerImpl.hpp"

/* OMRTODO Workaround to allow C++ libs to be linked without libstdc++ */
extern "C" {
void
__cxa_pure_virtual(void)
{
	abort();
}

void __pure_virtual()
{
	abort();
}

void __PureVirtualCalled()
{
	abort();
}
} /* extern "C" { */

void*
finalizeList_mallocWrapper(void *user, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit)
{
	return ruby_mimmalloc(size);
}

void
finalizeList_freeWrapper(void *user, void *ptr, uint32_t type)
{
	ruby_mimfree(ptr);
}

/* Random guesses */
#define FINALIZE_LIST_GROW_SIZE 1024
#define FINALIZE_FRAGMENT_SIZE 128

void
rb_omr_init_finalizeList(rb_thread_t * th, rb_vm_t * vm)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(th->_omrVMThread);
	for (int i = 0; i < RUBY_TYPE_COUNT; i++) {
		assert(vm->finalizeList[i] == NULL);
		vm->finalizeList[i] = pool_new(sizeof(void*), 10, 0, 0, OMR_GET_CALLSITE(), 0, finalizeList_mallocWrapper, finalizeList_freeWrapper, NULL);
	}

	/* TODO: write a proper MM_SublistPool::newInstance method for this in omr */
	/* Init SublistPool */
	MM_SublistPool *finalizeList =  (MM_SublistPool *)ruby_mimmalloc(sizeof(MM_SublistPool));
	vm->rDataParallelFinalizeList = finalizeList;
	finalizeList->initialize(env, MM_AllocationCategory::OTHER);
	finalizeList->setGrowSize(FINALIZE_LIST_GROW_SIZE);

	/* Init fragment */
	J9VMGC_SublistFragment *finalizeFragment = (J9VMGC_SublistFragment *)malloc(sizeof(J9VMGC_SublistFragment));
	vm->rDataParallelFinalizeFragment = finalizeFragment;
	finalizeFragment->count = 0;
	finalizeFragment->fragmentCurrent = NULL;
	finalizeFragment->fragmentTop = NULL;
	finalizeFragment->fragmentSize = (uintptr_t)FINALIZE_FRAGMENT_SIZE;
	finalizeFragment->parentList = vm->rDataParallelFinalizeList;

	omr_increment_objspace_profile_count();
}

void
rb_omr_free_finalizeList(rb_vm_t * vm)
{
	for (int i = 0; i < RUBY_TYPE_COUNT; i++) {
		pool_kill(vm->finalizeList[i]);
	}

	ruby_mimfree(vm->rDataParallelFinalizeList);

}

int
rb_omr_startup_gc(rb_vm_t * vm)
{
	intptr_t gcRet = -1;
	/* Disable collection on malloc threshold by default */
	vm->enableMallocGC = 0;

	/* Initialize the structures required for the heap and mutator model. */
	MM_StartupManagerImpl manager(vm->_omrVM);
	gcRet = OMR_GC_IntializeHeapAndCollector(vm->_omrVM, &manager);

	if (OMR_ERROR_NONE != omrthread_monitor_init_with_name(&vm->exclusiveAccessMutex, 0, "VM Exclusive Access Mutex")) {
		return OMR_ERROR_INTERNAL;
	}

	if (0 != gcRet) {
		return OMR_ERROR_INTERNAL;
	}

	return OMR_ERROR_NONE;
}

int
rb_omr_startup_gc_threads(rb_thread_t * th)
{
	return OMR_GC_InitializeDispatcherThreads(th->_omrVMThread);
}

int
rb_omr_gc_shutdown_dispatcher_threads(rb_thread_t * th)
{
	return OMR_GC_ShutdownDispatcherThreads(th->_omrVMThread);
}

int
rb_omr_gc_shutdown_collector(rb_thread_t * th)
{
	return OMR_GC_ShutdownCollector(th->_omrVMThread);
}

int
rb_omr_gc_shutdown_heap(rb_vm_t *vm)
{
	return OMR_GC_ShutdownHeap(vm->_omrVM);
}

int
rb_omr_startup_traceengine(rb_vm_t *vm)
{
	omr_error_t error = OMR_ERROR_NONE;
	/* Take trace options from OMR_TRACE_OPTIONS */
	const char *traceOpts = getenv("OMR_TRACE_OPTIONS");
	if (NULL != traceOpts) {
		error = omr_ras_initTraceEngine(vm->_omrVM, traceOpts, ".");
	}

	return error;
}

/* Load healthcenter agent
 * Precondition: vm->_omrVM is initialized
 */
int
rb_omr_startup_healthcenter(rb_vm_t *vm)
{
	omr_error_t error = OMR_ERROR_NONE;
	/* Take agent options from OMR_AGENT_OPTIONS */
	const char *healthCenterOpt = getenv("OMR_AGENT_OPTIONS");
	if ((NULL != healthCenterOpt) && ('\0' != *healthCenterOpt)) {
		error = (omr_error_t)rb_omr_startup_methodDictionary(vm);
		if (OMR_ERROR_NONE == error) {
			error = omr_ras_initHealthCenter(vm->_omrVM, &(vm->_omrVM->_hcAgent), healthCenterOpt);
		}
	}

	return error;
}

/*
 * Load the blob file and initialise the eye catchers and points of interest
 * for DDR.
 *
 * Loading the blob file will only occur if the file is found.
 * Setting up the eye catchers will occur even if it isn't as you can force ddr
 * to use a particular blob file even if one wasn't in the core but can't access
 * the dump at all without the eye catchers to act as a starting point.
 *
 * None of the memory for the J9RAS structure, blob or points of interest is ever
 * freed. This ensures it is available even if the VM crashes during shutdown.
 */

#define POI_COUNT 3

void
ruby_init_ddr(rb_vm_t *vm)
{
	J9RAS *j9ras = NULL;
	RubyPOIListEntry *poiList = NULL;
	RubyPOIListEntry *ruby_current_vmPoi = NULL;
	RubyPOIListEntry *rb_cObjectPoi = NULL;
	RubyPOIListEntry *rb_global_symbolsPoi = NULL;
	void *blob = NULL;
	uintptr_t blobHandle = NULL;
	uintptr_t rc = 0;
	OMRPORT_ACCESS_FROM_OMRVM(vm->_omrVM);

	j9ras = (J9RAS*) omrmem_allocate_memory(sizeof(J9RAS), OMRMEM_CATEGORY_VM);
	if (NULL == j9ras) {
		return;
	}

	/* Allocate the linked list in one block for simplicity if the allocation fails.
	 * We still use it as a linked list as this is what DDR expects, it makes it
	 * easy to detect the last entry and allows us to change the allocation later on
	 * without impacting DDR if we need to.
	 */
	poiList = (RubyPOIListEntry*) omrmem_allocate_memory(POI_COUNT * sizeof(RubyPOIListEntry), OMRMEM_CATEGORY_VM);
	if (NULL == poiList) {
		omrmem_free_memory(j9ras);
		return;
	}

	ruby_current_vmPoi = &poiList[0];
	rb_cObjectPoi = &poiList[1];
	rb_global_symbolsPoi = &poiList[2];

	ruby_current_vmPoi->poiType = RUBY_VM_POI;
	strncpy(ruby_current_vmPoi->description, "ruby_current_vm", sizeof(ruby_current_vmPoi->description));
	ruby_current_vmPoi->poiPtr = (void *) &ruby_current_vm;
	ruby_current_vmPoi->next = rb_cObjectPoi;

	rb_cObjectPoi->poiType = RUBY_COBJECT_POI;
	strncpy(rb_cObjectPoi->description, "rb_cObject", sizeof(rb_cObjectPoi->description));
	/* We need to save the address of rb_cObject not the value as it hasn't been initialised yet. */
	rb_cObjectPoi->poiPtr = (void *) &rb_cObject;
	rb_cObjectPoi->next = rb_global_symbolsPoi;

	rb_global_symbolsPoi->poiType = GLOBAL_SYMTAB_POI;
	strncpy(rb_global_symbolsPoi->description, "global_symbols", sizeof(rb_global_symbolsPoi->description));
	rb_global_symbolsPoi->poiPtr = rb_global_symbols();
	rb_global_symbolsPoi->next = NULL;

	/* Open library for the ddr blob. (We use a library simply
	 * so we can locate it on the library path. Guessing the
	 * install directory is actually quite difficult.)
	 */
	rc = omrsl_open_shared_library((char*)"omrblob", &blobHandle, OMRPORT_SLOPEN_DECORATE | OMRPORT_SLOPEN_LAZY);

	if (0 == rc) {
		size_t (*getBlobSize)(void) = NULL;
		const unsigned char * (*getBlobData)(void) = NULL;
		uintptr_t errorSize = 0;
		uintptr_t errorData = 0;

		errorSize = omrsl_lookup_name(blobHandle, (char*)"getBlobSize", (uintptr_t *) &getBlobSize, "i");
		errorData = omrsl_lookup_name(blobHandle, (char*)"getBlobData", (uintptr_t *) &getBlobData, "v");

		if ((0 == errorSize) && (0 == errorData)) {
			size_t blobSize = getBlobSize();
			const unsigned char* blobData = getBlobData();
			/* We need to make sure the blob data is in writable memory
			 * otherwise Linux won't necessarily include it in a core
			 * file. (Because it assumes the libraries will be available
			 * at debug time.)
			 */
			blob = omrmem_allocate_memory(blobSize, OMRMEM_CATEGORY_VM);
			if (NULL != blob) {
				memcpy(blob, blobData, blobSize);
			}
		}
		/* Close the shared library again. We don't need it again.
		 * We don't check the result - there's not much we can do if it won't close.
		 */
		omrsl_close_shared_library(blobHandle);
	}

	/* We can setup the eye catchers even if we fail to load
	 * the blob. It is possible to force DDR to use a blob
	 * from a separate file if necessary.
	 */
	memset(j9ras, 0, sizeof(J9RAS));
	strcpy(j9ras->id, "J9VMRAS");
	j9ras->check1 = 0xaa55aa55;
	j9ras->check2 = 0xaa55aa55;
	j9ras->version = 0x100000;
	j9ras->length = sizeof(J9RAS);
	j9ras->blob = blob;
	j9ras->poiList = ruby_current_vmPoi;
}

int
rb_omr_ras_shutdown(rb_vm_t *vm, rb_thread_t *th)
{
	/*
	 * RAS shutdown runs much earlier than the rest of OMR shutdown because
	 * it requires a valid current thread.
	 */
	omr_ras_cleanupHealthCenter(vm->_omrVM, &vm->_omrVM->_hcAgent);
	omr_ras_cleanupMethodDictionary(vm->_omrVM);
	omr_ras_cleanupTraceEngine(th->_omrVMThread);
	return OMR_ERROR_NONE;
}

int
rb_omr_thread_init(rb_thread_t *rubyThread, const char *threadName)
{
	int rc = OMR_Thread_Init(GET_VM()->_omrVM, rubyThread, &rubyThread->_omrVMThread, threadName);
	return rc;
}

void
omrGCAfterFork()
{
	OMR_VMThread *omrVMThread = GET_THREAD()->_omrVMThread;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	OMR_VM *omrVM = env->getOmrVM();
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	MM_CollectorLanguageInterfaceImpl *cli = (MM_CollectorLanguageInterfaceImpl *)extensions->collectorLanguageInterface;
	cli->flushCachesForGC(env);

	if (NULL != extensions->verboseGCManager) {
		((MM_VerboseManagerImpl*)extensions->verboseGCManager)->reconfigureVerboseGC(omrVM);
	}

	uintptr_t threadCount = extensions->dispatcher->threadCount();
	extensions->dispatcher->reinitAfterFork(env, threadCount);
}

void
rb_omr_prefork(rb_vm_t *vm)
{
	omr_vm_preFork(vm->_omrVM);
}

void
rb_omr_postfork_parent(rb_vm_t *vm)
{
	omr_vm_postForkParent(vm->_omrVM);
}

void
rb_omr_postfork_child(rb_vm_t *vm)
{
	omr_vm_postForkChild(vm->_omrVM);
}
