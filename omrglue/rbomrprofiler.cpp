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

#include "rbomrprofiler.h"

#include "node.h"
#include "omr.h"
#include "omrprofiler.h"

#define RB_OMR_PROF_METHOD_NAME_IDX 0
#define RB_OMR_PROF_CLASS_NAME_IDX 1
#define RB_OMR_PROF_FILE_NAME_IDX 2
#define RB_OMR_PROF_LINE_NUMBER_IDX 3

static void rb_omr_sampleStack(const rb_thread_t *th, const rb_control_frame_t *cfp);

static const size_t methodPropertyCount = 4;
static const char *methodPropertyNames[methodPropertyCount] = {
	"methodName",
	"className",
	"fileName",
	"lineNumber"
};

const char* const*
OMR_Glue_GetMethodDictionaryPropertyNames()
{
	return methodPropertyNames;
}

int
OMR_Glue_GetMethodDictionaryPropertyNum()
{
	return (int)methodPropertyCount;
}

/* This data structure overlays OMR_MethodDictionaryEntry */
typedef struct RB_OMR_MethodDictionaryEntry {
	const void *key;
	const char *propertyValues[methodPropertyCount];
} RB_OMR_MethodDictionaryEntry;

int
rb_omr_startup_methodDictionary(rb_vm_t *vm)
{
	return (int)omr_ras_initMethodDictionary(vm->_omrVM, methodPropertyCount, methodPropertyNames);
}

/**
 * @brief Sample the current thread's stack
 *
 * @param[in] th the current thread
 * @param[in] cfp the top control frame. Should be the same as th->cfp.
 */
static void
rb_omr_sampleStack(const rb_thread_t *th, const rb_control_frame_t *cfp)
{
	OMR_VMThread *omrVMThread = th->_omrVMThread;
	const rb_control_frame_t *end_cfp = RUBY_VM_END_CONTROL_FRAME(th);
	unsigned int frameDepth = 0;

	/* NOTE The bottom-most frame seems to have no me or iseq. */
	while (cfp < end_cfp) {
		const void *methodKey = NULL;

		/* If cfp has an iseq, we will always trace the iseq.
		 * So me's that have iseqs are represented in the method dictionary
		 * only by their iseqs.
		 */
		if (NULL != cfp->iseq) {
			if ((ISEQ_TYPE_EVAL != cfp->iseq->type) && (NODE_IFUNC != nd_type((NODE*)cfp->iseq))) {
				methodKey = cfp->iseq;
			}
		} else if (NULL != cfp->me) {
			methodKey = cfp->me;
		}

		/* OMRTODO Is there anything useful to trace for frames that don't have an me or iseq? */
		if (NULL != methodKey) {
			if (0 == frameDepth) {
				omr_ras_sampleStackTraceStart(omrVMThread, methodKey);
			} else {
				omr_ras_sampleStackTraceContinue(omrVMThread, methodKey);
			}
		}
		++frameDepth;
		cfp = RUBY_VM_PREVIOUS_CONTROL_FRAME(cfp);
	}
}

void
rb_omr_checkSampleStack(const rb_thread_t *th, const uint32_t decrement)
{
	if (th->_omrVMThread->_sampleStackBackoff == 0) {
		th->_omrVMThread->_sampleStackBackoff = RB_OMR_SAMPLESTACK_BACKOFF_MAX;
		if (omr_ras_sampleStackEnabled()) {
			rb_omr_sampleStack(th, th->cfp);
		}
	}
	if (decrement > th->_omrVMThread->_sampleStackBackoff) {
		th->_omrVMThread->_sampleStackBackoff = 0;
	} else {
		th->_omrVMThread->_sampleStackBackoff -= decrement;
	}
}

void
rb_omr_insertMethodEntryInMethodDictionary(const rb_vm_t *vm, const rb_method_entry_t *me)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_VM *omrVM = (OMR_VM *)vm->_omrVM;

	if (NULL != omrVM->_methodDictionary) {
		RB_OMR_MethodDictionaryEntry tempEntry;

		if ((NULL != me->def) && (me->def->type == VM_METHOD_TYPE_ISEQ)) {
			/* See rb_omr_sampleStack().
			 * If a stack frame's method entry has an iseq, we will always trace the iseq.
			 * So me's that have iseqs are represented in the method dictionary
			 * only by their iseqs.
			 */
			rb_omr_insertISeqInMethodDictionary(vm, me->def->body.iseq);
			return;
		}

		memset(&tempEntry, 0, sizeof(tempEntry));
		tempEntry.key = me;
		if (NULL != me->def) {
			tempEntry.propertyValues[RB_OMR_PROF_METHOD_NAME_IDX] = rb_id2name(me->def->original_id);
		} else {
			/* OMRTODO Shouldn't get here? */
			tempEntry.propertyValues[RB_OMR_PROF_METHOD_NAME_IDX] = rb_id2name(me->called_id);
		}
		/* OMRTODO Is className correct?
		 * rb_class2name() crashes if me->klass == rb_cBasicObject.
		 * rb_class_path() doesn't crash, but doesn't attempt to find the super class of me->klass.
		 */
		VALUE classNameObj = rb_class_path(me->klass);
		tempEntry.propertyValues[RB_OMR_PROF_CLASS_NAME_IDX] = (Qnil != classNameObj)? RSTRING_PTR(classNameObj) : "<unknown class>";
		tempEntry.propertyValues[RB_OMR_PROF_FILE_NAME_IDX] = NULL;
		tempEntry.propertyValues[RB_OMR_PROF_LINE_NUMBER_IDX] = NULL;

#if 0
		/* OMRTODO Should any of the following me types be handled?
		 * Most of the unimplemented cases are omitted from vm backtraces.
		 */
		if (NULL != me->def) {
			switch (me->def->type) {
			case VM_METHOD_TYPE_ISEQ:
				/* Unreachable. The iseq was already inserted into the method dictionary above. */
				break;
			case VM_METHOD_TYPE_CFUNC:
				/* fileName is unknown */
				break;
			case VM_METHOD_TYPE_ATTRSET:
			case VM_METHOD_TYPE_IVAR:
				break;
			case VM_METHOD_TYPE_BMETHOD:
				break;
			case VM_METHOD_TYPE_ZSUPER:
				break;
			case VM_METHOD_TYPE_UNDEF:
				break;
			case VM_METHOD_TYPE_NOTIMPLEMENTED:
				break;
			case VM_METHOD_TYPE_OPTIMIZED: /* Kernel#send, Proc#call, etc */
				break;
			case VM_METHOD_TYPE_MISSING: /* wrapper for method_missing(id) */
				break;
			case VM_METHOD_TYPE_REFINED:
				break;
			default:
				break;
			}
		}
#endif

		rc = omr_ras_insertMethodDictionary(omrVM, (OMR_MethodDictionaryEntry *)&tempEntry);
		if (OMR_ERROR_NONE != rc) {
			rb_bug("rb_omr_insertMethodEntryInMethodDictionary failed\n");
		}
	}
}

void
rb_omr_insertISeqInMethodDictionary(const rb_vm_t *vm, const rb_iseq_t *iseq)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_VM *omrVM = (OMR_VM *)vm->_omrVM;

	if (NULL != omrVM->_methodDictionary) {
		VALUE iseqSelf = iseq->self;
		VALUE methodName = rb_iseq_method_name(iseqSelf);
		RB_OMR_MethodDictionaryEntry tempEntry;
		OMRPORT_ACCESS_FROM_OMRVM(omrVM);
		char lineno[30] = "";

		memset(&tempEntry, 0, sizeof(tempEntry));
		tempEntry.key = iseq;
		/* OMRTODO Intern the "<unnamed>" string */
		tempEntry.propertyValues[RB_OMR_PROF_METHOD_NAME_IDX] = (Qnil != methodName)? RSTRING_PTR(methodName) : "<unnamed>";
		tempEntry.propertyValues[RB_OMR_PROF_CLASS_NAME_IDX] = rb_class2name(iseq->local_iseq->klass);
		tempEntry.propertyValues[RB_OMR_PROF_FILE_NAME_IDX] = RSTRING_PTR(iseq->location.path);
		/* OMRTODO Handle case where line number is >= 30 digits */
		omrstr_printf(lineno, sizeof(lineno), "%zu", (uintptr_t)rb_iseq_line_no(iseq, 0));
		tempEntry.propertyValues[RB_OMR_PROF_LINE_NUMBER_IDX] = lineno;

		rc = omr_ras_insertMethodDictionary(omrVM, (OMR_MethodDictionaryEntry *)&tempEntry);
		if (OMR_ERROR_NONE != rc) {
			rb_bug("omr_ras_insertISeqInMethodDictionary failed\n");
		}
	}
}

