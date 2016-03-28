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

#ifndef RBOMRINIT_H_
#define RBOMRINIT_H_

#include "ruby.h"
#include "vm_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void rb_omr_init_finalizeList(rb_thread_t *th, rb_vm_t * vm);
void rb_omr_free_finalizeList(rb_vm_t * vm);

int rb_omr_startup_gc(rb_vm_t * vm);
int rb_omr_startup_gc_threads(rb_thread_t * th);
int rb_omr_gc_shutdown_dispatcher_threads(rb_thread_t * th);
int rb_omr_gc_shutdown_collector(rb_thread_t * th);
int rb_omr_gc_shutdown_heap(rb_vm_t *vm);

int rb_omr_startup_traceengine(rb_vm_t *vm);
int rb_omr_startup_healthcenter(rb_vm_t *vm);

void ruby_init_ddr(rb_vm_t *vm);

int rb_omr_ras_shutdown(rb_vm_t *vm, rb_thread_t *th);

/* Wrapper necessary to deal with TLS-related Ruby specific actions */
int rb_omr_thread_init(rb_thread_t * rubyThread, const char * threadName);

void omrGCAfterFork(void);

void rb_omr_prefork(rb_vm_t *vm);
void rb_omr_postfork_parent(rb_vm_t *vm);
void rb_omr_postfork_child(rb_vm_t *vm);

#ifdef __cplusplus
} /* extern "C" { */
#endif

#endif /* RBOMRINIT_H_ */
