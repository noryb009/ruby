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

#include "CompactSchemeFixupObject.hpp"

#include "objectdescription.h"

#include "EnvironmentStandard.hpp"

void
MM_CompactSchemeFixupObject::fixupObject(MM_EnvironmentStandard *env, omrobjectptr_t objectPtr)
{
}


void
MM_CompactSchemeFixupObject::verifyForwardingPtr(omrobjectptr_t objectPtr, omrobjectptr_t forwardingPtr)
{
	assume0(forwardingPtr <= objectPtr);
}
