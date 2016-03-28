/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

/* Because this file is compiled with gcc -ansi, we need to explicitly enable POSIX
 * and GNU features to allow omr.h to compile. In particular, omr.h needs access to the
 * definitions for pthread_spinlock_t and stack_t.
 */
#define _GNU_SOURCE 1
#include "omr.h"

#if defined(WIN32)
omr_error_t
OMR_Glue_GetVMDirectoryToken(void **token)
{
	/* NULL means the runtime will look in the current executable's directory */
	*token = NULL;
	return OMR_ERROR_NONE;
}
#endif /* defined(WIN32) */

/**
 * Provides the thread name to be used when no name is given.
 */
char *
OMR_Glue_GetThreadNameForUnamedThread(OMR_VMThread *vmThread)
{
	return "(unnamed thread)";
}
