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

#if !defined(OBJECTDESCRIPTION_H_)
#define OBJECTDESCRIPTION_H_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omr.h"

/**
 * Object token definitions to be used by OMR components (and eventually subsume j9object_t and related).
 */
typedef void* languageobjectptr_t;
typedef void* omrobjectptr_t;
typedef void* omrarrayptr_t;

#if defined (OMR_GC_COMPRESSED_POINTERS)
typedef uint32_t fomrobject_t;
typedef uint32_t fomrarray_t;
#else
typedef uintptr_t fomrobject_t;
typedef uintptr_t fomrarray_t;
#endif

/**
 * Object header used by OMR to prefix all Ruby objects. Normally the object size would be stored here, but all
 * objects are currently cell-sized, so we do not store the size at all. The object table handle is properly
 * aligned and will have 0 in the two lowest significance bits, thus not being confused with a hole when
 * iterating over the heap. In the future we can share a single 64 bit word to store size and the handle offset.
 */
#endif /* OBJECTDESCRIPTION_H_ */
