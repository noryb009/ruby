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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(OBJECTITERATOR_HPP_)
#define OBJECTITERATOR_HPP_

#include "ModronAssertions.h"
#include "modronbase.h"

#include "Base.hpp"
#include "GCExtensionsBase.hpp"
#include "objectdescription.h"
#include "ObjectModel.hpp"
#include "SlotObject.hpp"


class GC_ObjectIterator
{
/* Data Members */
private:
    GC_SlotObject _slotObject;  /**< Create own SlotObject class to provide output */

protected:
public:

/* Member Functions */
private:
protected:
public:

    /**
     * Return back SlotObject for next reference
     * @return SlotObject
     */
    MMINLINE GC_SlotObject *nextSlot()
    {
        Assert_MM_unimplemented();
        return NULL;
    }

    /**
     * @param omrVM[in] pointer to the OMR VM
     * @param objectPtr[in] the object to be processed
     */
    GC_ObjectIterator(OMR_VM *omrVM, omrobjectptr_t objectPtr)
        : _slotObject(GC_SlotObject(omrVM, NULL))
      {

      }
};

#endif /* OBJECTITERATOR_HPP_ */
