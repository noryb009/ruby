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

#ifndef RBOMRPROFILER_H_
#define RBOMRPROFILER_H_

#include "ruby.h"
#include "vm_core.h"
#include "iseq.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This file defines the interface used by Ruby code to access omrglue/ profiling-related functions. */


/**
 * @brief Initialize the method dictionary.
 *
 * @param[in,out] vm The Ruby VM
 *
 * @return An OMR error code.
 */
int rb_omr_startup_methodDictionary(rb_vm_t *vm);

/**
 * @brief Insert an rb_method_entry_t into the method dictionary.
 *
 * @param[in] vm The Ruby VM
 * @param[in] me The method entry
 *
 * Note: The caller doesn't have its current thread context.
 */
void rb_omr_insertMethodEntryInMethodDictionary(const rb_vm_t *vm, const rb_method_entry_t *me);

/**
 * @brief Insert an rb_iseq_t into the method dictionary.
 *
 * @param[in] vm The Ruby VM
 * @param[in] iseq The instruction sequence
 *
 * Note: The caller doesn't have its current thread context.
 */
void rb_omr_insertISeqInMethodDictionary(const rb_vm_t *vm, const rb_iseq_t *iseq);

/**
 * @brief Sample the current thread's stack.
 *
 * If the sampling backoff has reached 0, sample the current thread's stack.
 * Decrement the sampling backoff.
 *
 * @param[in] th The current thread
 * @param[in] decrement The amount to decrement the sampling backoff
 */
void rb_omr_checkSampleStack(const rb_thread_t *th, const uint32_t decrement);

#define RB_OMR_SAMPLESTACK_BACKOFF_MAX 100			/* Max value of the stack sampling backoff counter */
#define RB_OMR_SAMPLESTACK_BACKOFF_TIMER_DECR 100	/* Sampling backoff decrement for timer interrupt */

#ifdef __cplusplus
} /* extern "C" { */
#endif
#endif /* RBOMRPROFILER_H_ */
