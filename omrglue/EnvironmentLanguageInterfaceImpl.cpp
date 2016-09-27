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

#include "omrcfg.h"

#include "omrlinkedlist.h"

#include "Collector.hpp"
#include "EnvironmentLanguageInterfaceImpl.hpp"
#include "EnvironmentStandard.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "OMRVMInterface.hpp"

#include "vm_core.h"
#include "vmaccess.h"


MM_EnvironmentLanguageInterfaceImpl::MM_EnvironmentLanguageInterfaceImpl(MM_EnvironmentBase *env)
	: MM_EnvironmentLanguageInterface(env)
{
	_typeId = __FUNCTION__;
};

MM_EnvironmentLanguageInterfaceImpl *
MM_EnvironmentLanguageInterfaceImpl::newInstance(MM_EnvironmentBase *env)
{
	MM_EnvironmentLanguageInterfaceImpl *eliRuby = NULL;

	eliRuby = (MM_EnvironmentLanguageInterfaceImpl *)env->getForge()->allocate(sizeof(MM_EnvironmentLanguageInterfaceImpl), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != eliRuby) {
		new(eliRuby) MM_EnvironmentLanguageInterfaceImpl(env);
		if (!eliRuby->initialize(env)) {
			eliRuby->kill(env);
			eliRuby = NULL;
		}
	}

	return eliRuby;
}

void
MM_EnvironmentLanguageInterfaceImpl::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_EnvironmentLanguageInterfaceImpl::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_EnvironmentLanguageInterfaceImpl::tearDown(MM_EnvironmentBase *env)
{
}

/* **************************************************************************************
 *               Description of VM Access and Exclusive Access Mechanism                *
 * **************************************************************************************
 *
 * Ruby has a cooperative single-lock (GVL) threading model. Whenever a thread is executing
 * Ruby code, it *must* hold the GVL. For performance reasons we have replaced several
 * malloc-style allocations (e.g. string data) with T_OMRBUF heap allocated objects.
 * This means that Ruby async threads (threads executing without GIL, for example by
 * use of GVL_UNLOCK_BEGIN(), can now allocate on the heap and possibly trigger GCs.
 *
 * Unlike Java, the VM Access mechanism cannot preempt a thread or explicitly request
 * that it give up VM Access, instead the Exclusive requesting thread will wait until
 * the threads with VM Access give it up.
 */

void
MM_EnvironmentLanguageInterfaceImpl::acquireVMAccess()
{
	rb_thread_t *thr = (rb_thread_t*) _env->getOmrVMThread()->_language_vmthread;

#if defined(DEBUG_VM_ACCESS) && 0
	fprintf(stderr, "acquireVMAccess [%p]\n",thr);
#endif /* DEBUG_VM_ACCESS */

	Assert_DoesNotHave_VM_Access(thr);

	/* Try fast-path acquire */
	if (0 != VM_AtomicSupport::lockCompareExchange(&thr->publicFlags, 0, RUBY_THREAD_VM_ACCESS)) {
		/* Fast-path failed, exclusive request must be pending. Try slow path. */
		omrthread_monitor_enter_using_threadId(thr->publicFlagsMutex, thr->_omrVMThread->_os_thread);
		/* Wait for exclusive to be released, if necessary. */
		while (thr->publicFlags & RUBY_THREAD_HALT_FOR_EXCLUSIVE) {
			omrthread_monitor_wait(thr->publicFlagsMutex);
		}
		/* Grab VM Access */
		VM_AtomicSupport::bitOr(&thr->publicFlags, RUBY_THREAD_VM_ACCESS);
		omrthread_monitor_exit_using_threadId(thr->publicFlagsMutex, thr->_omrVMThread->_os_thread);
	}

	Assert_MM_false(RUBY_XACCESS_EXCLUSIVE == ((rb_vm_t*) _env->getOmrVM()->_language_vm)->exclusiveAccessState);
}

void
MM_EnvironmentLanguageInterfaceImpl::releaseVMAccess()
{
	rb_thread_t *thr = (rb_thread_t*) _env->getOmrVMThread()->_language_vmthread;

#if defined(DEBUG_VM_ACCESS) && 0
	fprintf(stderr, "releaseVMAccess [%p]\n",thr);
#endif /* DEBUG_VM_ACCESS */

	Assert_Has_VM_Access(thr);

	/* Try fast-path release */
	if (RUBY_THREAD_VM_ACCESS != VM_AtomicSupport::lockCompareExchange(&thr->publicFlags, RUBY_THREAD_VM_ACCESS, 0)) {
		/* Fast-path failed, exclusive request must be pending. Try slow path. */
		omrthread_monitor_enter_using_threadId(thr->publicFlagsMutex, thr->_omrVMThread->_os_thread);
		/* Clear VM Access */
		VM_AtomicSupport::bitAnd(&thr->publicFlags, ~RUBY_THREAD_VM_ACCESS);
		/* Wake up threads waiting for exclusive */
		if (thr->publicFlags & RUBY_THREAD_HALT_FOR_EXCLUSIVE) { /* RUBY_THREAD_HALT_FOR_EXCLUSIVE is cleared by releaseExclusiveVMAccess(...) */
			rb_vm_t *vm = (rb_vm_t*) _env->getOmrVM()->_language_vm;
			omrthread_monitor_enter_using_threadId(vm->exclusiveAccessMutex, thr->_omrVMThread->_os_thread);
			--vm->exclusiveAccessResponseCount;
#if defined(DEBUG_VM_ACCESS)
			fprintf(stderr,"releaseVMAccess -> count %lu on rb_thread_t %p\n",vm->exclusiveAccessResponseCount,thr);
#endif /* DEBUG_VM_ACCESS */
			if (vm->exclusiveAccessResponseCount == 0) {
				omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
			}
			omrthread_monitor_exit(vm->exclusiveAccessMutex);
		}
		omrthread_monitor_exit_using_threadId(thr->publicFlagsMutex, thr->_omrVMThread->_os_thread);
	}
}

/**
 * Try and acquire exclusive access if no other thread is already requesting it.
 * Make an attempt at acquiring exclusive access if the current thread does not already have it.  The
 * attempt will abort if another thread is already going for exclusive, which means this
 * call can return without exclusive access being held.  As well, this call will block for any other
 * requesting thread, and so should be treated as a safe point.
 * @note call can release VM access.
 * @return true if exclusive access was acquired, false otherwise.
 */
bool
MM_EnvironmentLanguageInterfaceImpl::tryAcquireExclusiveVMAccess()
{
	acquireExclusiveVMAccess();
	return true;
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
bool
MM_EnvironmentLanguageInterfaceImpl::tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats)
{
	Assert_MM_unreachable(); /* Implementation will be provided with concurrent GC support later. */
	return true;
}

void
MM_EnvironmentLanguageInterfaceImpl::releaseExclusiveForConcurrentKickoff()
{
	Assert_MM_unreachable();  /* Implementation will be provided with concurrent GC support later. */
	return;
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

/**
 * Acquires exclusive VM access
 */
void
MM_EnvironmentLanguageInterfaceImpl::acquireExclusiveVMAccess()
{
#if defined(DEBUG_VM_ACCESS)
	fprintf(stderr, "acquireExclusiveVMAccess env [%p]\n",_env);
#endif /* DEBUG_VM_ACCESS */

	rb_vm_t *vm = (rb_vm_t*) _env->getOmrVM()->_language_vm;
	rb_thread_t *currentThr = (rb_thread_t*) _env->getOmrVMThread()->_language_vmthread;

	Assert_Has_VM_Access(currentThr);

	_env->getOmrVMThread()->exclusiveCount = 1;
	uintptr_t responsesExpected = 0;

	omrthread_monitor_enter(vm->exclusiveAccessMutex);
	/* Case 1: First thread to go for exclusive access. */
	if (RUBY_XACCESS_NONE == vm->exclusiveAccessState) {
		vm->exclusiveAccessResponseCount = 0;

		omrthread_monitor_enter(_env->getOmrVM()->_vmThreadListMutex);

		OMR_VMThread * listThread = _env->getOmrVMThread();
		while ((listThread = listThread->_linkNext) != _env->getOmrVMThread()) {
			rb_thread_t *thr = (rb_thread_t*) listThread->_language_vmthread;
			omrthread_monitor_enter(thr->publicFlagsMutex);
			VM_AtomicSupport::bitOr(&thr->publicFlags, RUBY_THREAD_HALT_FOR_EXCLUSIVE);
			if (thr->publicFlags & RUBY_THREAD_VM_ACCESS) {
				responsesExpected++; /* Only wait for threads that were actually running. */
#if defined(DEBUG_VM_ACCESS)
				fprintf(stderr,"acquireExclusiveVMAccess waiting on -> count %lu on rb_thread_t %p\n",responsesExpected,thr);
#endif /* DEBUG_VM_ACCESS */
			}
			omrthread_monitor_exit(thr->publicFlagsMutex);
		}

		/* WARNING: This means that the other executing threads should pause themselves and release VM Access before spawning more threads! */
		omrthread_monitor_exit(_env->getOmrVM()->_vmThreadListMutex);
	}
	/* Case 2: Add to queue for exclusive access. */
	else {
		/* Queue Management */
		if ( J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
			J9_LINEAR_LINKED_LIST_ADD(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,currentThr);
		} else {
			J9_LINEAR_LINKED_LIST_ADD_AFTER(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,vm->exclusiveVMAccessQueueTail,currentThr);
		}
		/* Update the tail, because the macros don't do this. */
		vm->exclusiveVMAccessQueueTail = currentThr;

		/* Set current thread as queued, also set it as halted so acquireVMAccess() can work correctly. */
		omrthread_monitor_enter(currentThr->publicFlagsMutex);
		VM_AtomicSupport::bitOr(&currentThr->publicFlags, RUBY_THREAD_QUEUED_FOR_EXCLUSIVE | RUBY_THREAD_HALT_FOR_EXCLUSIVE);
		omrthread_monitor_exit(currentThr->publicFlagsMutex);

		/* Set global state */
		vm->exclusiveAccessState = RUBY_XACCESS_PENDING;
		omrthread_monitor_exit(vm->exclusiveAccessMutex);

		/* Wait on the public flags mutex in acquireVMAccess(), when the thread gets notified, it should continue to obtain exclusive VM access. */
		releaseVMAccess();
		acquireVMAccess(); /* Will only return once RUBY_THREAD_HALT_FOR_EXCLUSIVE has been removed by releaseExclusiveVMAccess() */

		/* Unset current thread as queued */
		omrthread_monitor_enter(currentThr->publicFlagsMutex);
		VM_AtomicSupport::bitAnd(&currentThr->publicFlags, ~RUBY_THREAD_QUEUED_FOR_EXCLUSIVE);
		omrthread_monitor_exit(currentThr->publicFlagsMutex);

		/* Set global state */
		omrthread_monitor_enter(vm->exclusiveAccessMutex);

		if (RUBY_XACCESS_HANDING_OFF == vm->exclusiveAccessState ) {
			/* Wait for the thread that handed off to give up any access it holds.
			 * It will definitely be holding VM access, and releaseExclusiveVMAccess
			 * will adjust the count accordingly. */
			responsesExpected = 1;
		}
		vm->exclusiveAccessState = RUBY_XACCESS_HANDED_OFF;
	}

	/* Common */

	/* Wait for all threads to respond to the halt request  */
	vm->exclusiveAccessResponseCount += responsesExpected;
	while(vm->exclusiveAccessResponseCount) { /* Decremented by individual threads releasing VM Access */
		omrthread_monitor_wait(vm->exclusiveAccessMutex);
	}
	vm->exclusiveAccessState = RUBY_XACCESS_EXCLUSIVE;

	omrthread_monitor_exit(vm->exclusiveAccessMutex);
	/* During exclusive we hold the _vmTheadListMutex to prevent root set changes. */
	omrthread_monitor_enter(_env->getOmrVM()->_vmThreadListMutex);
}

/**
 * Releases exclusive VM access.
 */
void
MM_EnvironmentLanguageInterfaceImpl::releaseExclusiveVMAccess()
{
#if defined(DEBUG_VM_ACCESS)
	fprintf(stderr, "releaseExclusiveVMAccess env [%p]\n",_env);
#endif /* DEBUG_VM_ACCESS */

	rb_thread_t *thr = (rb_thread_t*) _env->getOmrVMThread()->_language_vmthread;
	rb_vm_t *vm = (rb_vm_t*) _env->getOmrVM()->_language_vm;

	Assert_Has_VM_Access(thr);
	Assert_MM_false(0 == _env->getOmrVMThread()->exclusiveCount);
	Assert_MM_true(RUBY_XACCESS_EXCLUSIVE == vm->exclusiveAccessState);

	_env->getOmrVMThread()->exclusiveCount = 0;

	/* Acquire monitors in the same order as in thread_start_func_2 to prevent deadlock */

	/* Check the exclusive access queue */
	omrthread_monitor_enter(vm->exclusiveAccessMutex);

	/* Case 1: No other threads queued for exclusive access. */
	if ( J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {

		/* Queue is empty, so wake up all previously halted threads and notify the exclusiveAccessMutex */
		OMR_VMThread * listThread = _env->getOmrVMThread();

		vm->exclusiveAccessState = RUBY_XACCESS_NONE;
#if defined(DEBUG_VM_ACCESS)
		fprintf(stderr,"RUBY_XACCESS_NONE\n");
#endif /* DEBUG_VM_ACCESS */

		while ((listThread = listThread->_linkNext) != _env->getOmrVMThread()) {
			rb_thread_t *thr = (rb_thread_t*) listThread->_language_vmthread;
			omrthread_monitor_enter(thr->publicFlagsMutex);
			VM_AtomicSupport::bitAnd(&thr->publicFlags, ~RUBY_THREAD_HALT_FOR_EXCLUSIVE);
			omrthread_monitor_notify_all(thr->publicFlagsMutex); /* Wake up the thread waiting for VM Access */
			omrthread_monitor_exit(thr->publicFlagsMutex);
		}
	}
	/* Case 2: Other threads are queued for exclusive access. */
	else {
		rb_thread_t *nextThread = NULL;
		vm->exclusiveAccessState = RUBY_XACCESS_HANDING_OFF;
#if defined(DEBUG_VM_ACCESS)
		fprintf(stderr,"RUBY_XACCESS_HANDING_OFF\n");
#endif /* DEBUG_VM_ACCESS */

		/* Mark current thread as halted */
		omrthread_monitor_enter(thr->publicFlagsMutex);
		VM_AtomicSupport::bitOr(&thr->publicFlags, RUBY_THREAD_HALT_FOR_EXCLUSIVE);
		omrthread_monitor_exit(thr->publicFlagsMutex);

		/* Queue is nonempty so clear the halt flags on the next thread only */
		nextThread = vm->exclusiveVMAccessQueueHead;

		J9_LINEAR_LINKED_LIST_REMOVE(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,nextThread);
		/* If the queue is now empty, set the tail to NULL as well */
		if ( J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
			vm->exclusiveVMAccessQueueTail = NULL;
		}

		/* Make sure the thread we've dequeued isn't pointing to another thread in the queue
		 * now that it's no longer part of the linked list.
		 */
		nextThread->exclusiveVMAccessQueueNext = NULL;

		omrthread_monitor_enter(nextThread->publicFlagsMutex);
		VM_AtomicSupport::bitAnd(&nextThread->publicFlags, ~RUBY_THREAD_HALT_FOR_EXCLUSIVE);
		omrthread_monitor_notify_all(nextThread->publicFlagsMutex); /* Wake up the thread waiting in the VM Exclusive queue. */
		omrthread_monitor_exit(nextThread->publicFlagsMutex);
	}

	/* Common */

	omrthread_monitor_exit(vm->exclusiveAccessMutex);
	omrthread_monitor_exit(_env->getOmrVM()->_vmThreadListMutex);

	Assert_MM_true(RUBY_XACCESS_NONE == vm->exclusiveAccessState);
}

bool
MM_EnvironmentLanguageInterfaceImpl::saveObjects(omrobjectptr_t objectPtr)
{
	return true;
}

void
MM_EnvironmentLanguageInterfaceImpl::restoreObjects(omrobjectptr_t *objectPtrIndirect)
{
}

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Disable inline TLH allocates by hiding the real heap allocation address from
 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
 * looks full.
 *
 */
void
MM_EnvironmentLanguageInterfaceImpl::disableInlineTLHAllocate()
{
	rb_thread_t* rbthread = static_cast<rb_thread_t*>(_env->getLanguageVMThread());
		LanguageThreadLocalHeapStruct *tlh = (LanguageThreadLocalHeapStruct *)&rbthread->omrTlh.allocateThreadLocalHeap;
		tlh->realHeapAlloc = rbthread->omrTlh.heapAlloc;
		rbthread->omrTlh.heapAlloc = rbthread->omrTlh.heapTop;

	#if defined(OMR_GC_NON_ZERO_TLH)
		tlh = (LanguageThreadLocalHeapStruct *)&rbthread->omrTlh.nonZeroAllocateThreadLocalHeap;
		tlh->realHeapAlloc = rbthread->omrTlh.nonZeroHeapAlloc;
		rbthread->omrTlh.nonZeroHeapAlloc = rbthread->omrTlh.nonZeroHeapTop;
	#endif /* defined(OMR_GC_NON_ZERO_TLH) */
}

/**
 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
 */
void
MM_EnvironmentLanguageInterfaceImpl::enableInlineTLHAllocate()
{
	rb_thread_t* rbthread = static_cast<rb_thread_t*>(_env->getLanguageVMThread());
		LanguageThreadLocalHeapStruct *tlh = (LanguageThreadLocalHeapStruct *)&rbthread->omrTlh.allocateThreadLocalHeap;
		rbthread->omrTlh.heapAlloc =  tlh->realHeapAlloc;
		tlh->realHeapAlloc = NULL;

	#if defined(OMR_GC_NON_ZERO_TLH)
		tlh = (LanguageThreadLocalHeapStruct *)&rbthread->omrTlh.nonZeroAllocateThreadLocalHeap;
		rbthread->omrTlh.nonZeroHeapAlloc = tlh->realHeapAlloc;
		tlh->realHeapAlloc = NULL;
	#endif /* defined(OMR_GC_NON_ZERO_TLH) */
}

/**
 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otheriwse
 */
bool
MM_EnvironmentLanguageInterfaceImpl::isInlineTLHAllocateEnabled()
{
	rb_thread_t* rbthread = static_cast<rb_thread_t*>(_env->getLanguageVMThread());
	LanguageThreadLocalHeapStruct *tlh = (LanguageThreadLocalHeapStruct *)&rbthread->omrTlh.allocateThreadLocalHeap;
	return (NULL != tlh->realHeapAlloc) ? false : true;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

void
MM_EnvironmentLanguageInterfaceImpl::parallelMarkTask_setup(MM_EnvironmentBase *env)
{
}

void
MM_EnvironmentLanguageInterfaceImpl::parallelMarkTask_cleanup(MM_EnvironmentBase *envBase)
{
}
