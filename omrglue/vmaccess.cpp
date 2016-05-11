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
#include "omr.h"

#include "omrthread.h"
#include "omrgc.h"
#include "vmaccess.h"
#include "thread_api.h"
#include "ruby/ruby.h"
#include "vm_core.h"

#include "CollectorLanguageInterfaceImpl.hpp"
#include "GCExtensionsBase.hpp"

#include "EnvironmentBase.hpp"
#include "EnvironmentLanguageInterface.hpp"

void
acquire_vm_access(rb_thread_t * rubyThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(rubyThread->_omrVMThread);
	env->acquireVMAccess();
}

void
acquire_vm_access_without_gvl()
{
	acquire_vm_access((rb_thread_t*)omr_vmthread_getCurrent(GET_VM()->_omrVM)->_language_vmthread);
}

void
release_vm_access(rb_thread_t * rubyThread)
{
	Assert_MM_true(NULL != rubyThread->_omrVMThread);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(rubyThread->_omrVMThread);
	env->releaseVMAccess();

}

void
release_vm_access_without_gvl()
{
	release_vm_access((rb_thread_t*)omr_vmthread_getCurrent(GET_VM()->_omrVM)->_language_vmthread);
}

void
acquire_vm_exclusive_access(rb_thread_t * rubyThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(rubyThread->_omrVMThread);
	env->acquireExclusiveVMAccess();
}

void
release_vm_exclusive_access(rb_thread_t * rubyThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(rubyThread->_omrVMThread);
	env->releaseExclusiveVMAccess();
}

void
set_thread_flags(rb_thread_t * rubyThread, uintptr_t mask)
{
	omrthread_monitor_enter(rubyThread->publicFlagsMutex);
	VM_AtomicSupport::bitOr(&rubyThread->publicFlags, mask);
	omrthread_monitor_exit(rubyThread->publicFlagsMutex);
}
