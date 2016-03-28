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

#ifndef RBGCSUPPORT_H_
#define RBGCSUPPORT_H_

#include "vm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "gc.h"
#include "objectdescription.h"
#include "j9nongenerated.h"

int rb_omr_is_valid_object(VALUE rubyObject);
int rb_omr_mark_valid_object(rb_thread_t * rubyThread, VALUE rubyObject);

VALUE rb_omr_get_freeobj(rb_thread_t * th, size_t size);

VALUE rb_omr_convert_ref_to_id(VALUE object);
VALUE rb_omr_convert_id_to_ref(VALUE id);

void ruby_iterate_all_objects(void *objspace, OMR_VMThread *omrVMThread, void(*func)(RVALUE *p, void *objspace));
void rb_omr_each_objects(VALUE arg);
void rb_omr_count_objects(size_t * counts, size_t * freed, size_t * total);

rb_omr_markstate_t rb_omr_get_markstate(void);
void omr_mark_leaf_object(rb_omr_markstate_t ms, languageobjectptr_t object);


/*
 * Note that this just marks the omr object of the struct.  It does not perform scanning of the omr buf object.
 */
void mark_omrBufStruct(rb_omr_markstate_t ms, void *data);

void omr_mark_object(rb_omr_markstate_t ms, languageobjectptr_t object);

void rb_omr_iterate_all_objects(void *objspace, OMR_VMThread *omrVMThread, void(*func)(VALUE p, void *objspace));
void rb_omr_each_objects(VALUE arg);

void omr_onig_region_mark(rb_omr_markstate_t ms, struct re_registers *regs);
void omr_ext_strscan_free(void* ptr);

int rb_omr_prepare_heap_for_walk(void);

uintptr_t allocateMemoryForSublistFragment(void *vmThreadRawPtr, J9VMGC_SublistFragment *fragmentPrimitive);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* RBGCSUPPORT_H_ */
