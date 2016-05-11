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

#ifndef EnvironmentLanguageInterfaceImpl_HPP_
#define EnvironmentLanguageInterfaceImpl_HPP_

#include "omrcfg.h"
#include "omr.h"

#include "EnvironmentLanguageInterface.hpp"

#include "EnvironmentBase.hpp"

class MM_EnvironmentLanguageInterfaceImpl : public MM_EnvironmentLanguageInterface
{
private:
protected:
public:

private:
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_EnvironmentLanguageInterfaceImpl(MM_EnvironmentBase *env);

public:
	static MM_EnvironmentLanguageInterfaceImpl *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	static MM_EnvironmentLanguageInterfaceImpl *getInterface(MM_EnvironmentLanguageInterface *linterface) { return (MM_EnvironmentLanguageInterfaceImpl *)linterface; }

	virtual void acquireVMAccess();
	virtual void releaseVMAccess();

	virtual bool tryAcquireExclusiveVMAccess();
	virtual void acquireExclusiveVMAccess();
	virtual void releaseExclusiveVMAccess();

	virtual bool isExclusiveAccessRequestWaiting() { return false; }

	virtual bool saveObjects(omrobjectptr_t objectPtr);
	virtual void restoreObjects(omrobjectptr_t *objectPtrIndirect);

#if defined (OMR_GC_THREAD_LOCAL_HEAP)
	virtual void disableInlineTLHAllocate();
	virtual void enableInlineTLHAllocate();
	virtual bool isInlineTLHAllocateEnabled();
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void parallelMarkTask_setup(MM_EnvironmentBase *env);
	virtual void parallelMarkTask_cleanup(MM_EnvironmentBase *env);

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	virtual bool tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats);
	virtual void releaseExclusiveForConcurrentKickoff();
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */
};

#endif /* EnvironmentLanguageInterfaceImpl_HPP_ */
