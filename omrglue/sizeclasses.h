/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2006, 2016
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

#ifndef SIZECLASSES_H_
#define SIZECLASSES_H_

#include "omrcomp.h"

#if defined(OMR_GC_SEGREGATED_HEAP)

#define OMR_SIZECLASSES_NUM_SMALL 0x3F

#define OMR_SIZECLASSES_MIN_SMALL 0x01
#define OMR_SIZECLASSES_MAX_SMALL OMR_SIZECLASSES_NUM_SMALL
#define OMR_SIZECLASSES_ARRAYLET (OMR_SIZECLASSES_NUM_SMALL + 1)
#define OMR_SIZECLASSES_LARGE (OMR_SIZECLASSES_NUM_SMALL + 2)

#define OMR_SIZECLASSES_LOG_SMALLEST 0x04
#define OMR_SIZECLASSES_LOG_LARGEST 0x010
#define OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES (1 << OMR_SIZECLASSES_LOG_LARGEST)

/* The initial number of size classes and their sizes.
 */
#define SMALL_SIZECLASSES	{0, 16, 24, 32, 40, 48, 56, 64, 72, 88,\
								 104, 120, 136, 160, 184, 208, 240, 272, 312, 352,\
								 400, 456, 520, 592, 672, 760, 856, 968, 1096, 1240,\
								 1400, 1576, 1776, 2000, 2256, 2544, 2864, 3224, 3632, 4088,\
								 4600, 5176, 5824, 6552, 7376, 8304, 9344, 10512, 11832, 13312,\
								 14976, 16848, 18960, 21336, 24008, 27016, 30400, 34200, 38480, 43296,\
								 48712, 54808, 61664, 65536 }
typedef struct OMR_SizeClasses {
    uintptr_t smallCellSizes[OMR_SIZECLASSES_MAX_SMALL + 1];
    uintptr_t smallNumCells[OMR_SIZECLASSES_MAX_SMALL + 1];
    uintptr_t sizeClassIndex[OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES >> 2];
} OMR_SizeClasses;

#else /* OMR_GC_SEGREGATED_HEAP */
typedef struct OMR_SizeClasses {
    uintptr_t smallCellSizes[1];
    uintptr_t smallNumCells[1];
    uintptr_t sizeClassIndex[2];
} OMR_SizeClasses;

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SIZECLASSES_H_ */

