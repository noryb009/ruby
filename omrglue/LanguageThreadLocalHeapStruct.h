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

#ifndef LANGUAGETHREADLOCALHEAPSTRUCT_H
#define LANGUAGETHREADLOCALHEAPSTRUCT_H

#include "omr.h"

struct LanguageThreadLocalHeapStruct;

typedef struct LanguageThreadLocalHeapStruct {
	uint8_t* heapBase;
	uint8_t* realHeapAlloc;
	uintptr_t objectFlags;
	uintptr_t refreshSize;
	void* memorySubSpace;
	void* memoryPool;
} LanguageThreadLocalHeapStruct;

typedef struct omrTlhStruct {
	LanguageThreadLocalHeapStruct allocateThreadLocalHeap;
	LanguageThreadLocalHeapStruct nonZeroAllocateThreadLocalHeap;

	uint8_t* nonZeroHeapAlloc;
	uint8_t* heapAlloc;

	uint8_t* nonZeroHeapTop;
	uint8_t* heapTop;

	intptr_t nonZeroTlhPrefetchFTA;
	intptr_t tlhPrefetchFTA;
} omrTlhStruct;

#endif /* LANGUAGETHREADLOCALHEAPSTRUCT_H */
