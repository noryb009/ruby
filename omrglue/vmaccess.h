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

#ifndef VMACCESS_H_
#define VMACCESS_H_

#include "vm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef DEBUG_VM_ACCESS /* Enable printf debugging on VM (X)Access events */

#define RUBY_THREAD_VM_ACCESS 0x1
#define RUBY_THREAD_HALT_FOR_EXCLUSIVE 0x2
#define RUBY_THREAD_QUEUED_FOR_EXCLUSIVE 0x4

#define RUBY_XACCESS_NONE 0
#define RUBY_XACCESS_PENDING 1
#define RUBY_XACCESS_HANDING_OFF 2
#define RUBY_XACCESS_HANDED_OFF 3
#define RUBY_XACCESS_EXCLUSIVE 4

#define Assert_Has_VM_Access(thr) Assert_MM_true((thr)->publicFlags & RUBY_THREAD_VM_ACCESS)
#define Assert_DoesNotHave_VM_Access(thr) Assert_MM_false((thr)->publicFlags & RUBY_THREAD_VM_ACCESS)

void acquire_vm_access_without_gvl(void);
void release_vm_access_without_gvl(void);

void acquire_vm_access(rb_thread_t * rubyThread);
void release_vm_access(rb_thread_t * rubyThread);

void acquire_vm_exclusive_access(rb_thread_t * rubyThread);
void release_vm_exclusive_access(rb_thread_t * rubyThread);

void set_thread_flags(rb_thread_t * rubyThread, uintptr_t mask);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* VMACCESS_H_ */
