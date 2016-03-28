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

#if !defined(LANGUAGESEGREGATEDALLOCATIONCACHESTRUCT_H)
#define LANGUAGESEGREGATEDALLOCATIONCACHESTRUCT_H

#include "omr.h"
#include "sizeclasses.h"

#if defined(OMR_GC_SEGREGATED_HEAP)

typedef struct LanguageSegregatedAllocationCacheEntryStruct {
	uintptr_t* current;
	uintptr_t* top;
} LanguageSegregatedAllocationCacheEntryStruct;

typedef LanguageSegregatedAllocationCacheEntryStruct LanguageSegregatedAllocationCache[OMR_SIZECLASSES_NUM_SMALL + 1];

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* LANGUAGESEGREGATEDALLOCATIONCACHESTRUCT_H */

